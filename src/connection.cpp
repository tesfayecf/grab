#include "connection.hpp"
#include <iostream>
#include <sstream>
#include <regex>

Connection::Connection(net::io_context& ioc, const Config& config)
    : ioc_(ioc)
    , config_(config)
    , is_connected_(false)
    , is_connecting_(false)
    , auto_reconnect_(true)
    , should_stop_(false)
{
    // Initialize SSL context
    if (config_.use_ssl) {
        ssl_ctx_ = std::make_unique<ssl::context>(ssl::context::tlsv12_client);
        ssl_ctx_->set_default_verify_paths();
        ssl_ctx_->set_verify_mode(ssl::verify_peer);
    }

    // Initialize timers
    reconnect_timer_ = std::make_unique<net::steady_timer>(ioc_);
    connection_timer_ = std::make_unique<net::steady_timer>(ioc_);
    ping_timer_ = std::make_unique<net::steady_timer>(ioc_);
}

Connection::~Connection() {
    stop();
}

bool Connection::connect() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (is_connected_ || is_connecting_) {
        return is_connected_;
    }

    try {
        // Create new WebSocket stream
        if (config_.use_ssl) {
            ws_ = std::make_unique<WebSocketStream>(ioc_, *ssl_ctx_);
        } else {
            // For non-SSL connections, we'd need a different stream type
            // For now, assuming SSL is always used for Binance
            throw std::runtime_error("Non-SSL connections not implemented");
        }

        is_connecting_ = true;
        do_connect();
        return true;

    } catch (const std::exception& e) {
        handle_connection_error("Failed to initiate connection: " + std::string(e.what()));
        return false;
    }
}

void Connection::disconnect() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!is_connected_) {
        return;
    }

    try {
        // Cancel all timers
        reconnect_timer_->cancel();
        connection_timer_->cancel();
        ping_timer_->cancel();

        // Close WebSocket connection
        if (ws_ && ws_->is_open()) {
            ws_->close(websocket::close_code::normal);
        }

        is_connected_ = false;
        is_connecting_ = false;

        if (on_disconnected_) {
            on_disconnected_();
        }

    } catch (const std::exception& e) {
        if (error_callback_) {
            error_callback_("Error during disconnect: " + std::string(e.what()));
        }
    }
}

void Connection::subscribe_stream(const std::string& stream_name, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    stream_callbacks_[stream_name] = std::move(callback);
    
    std::cout << "Subscribed to stream: " << stream_name << " (total streams: " << stream_callbacks_.size() << ")" << std::endl;

    // If already connected, send individual subscription message
    if (is_connected_) {
        std::ostringstream json;
        json << "{";
        json << "\"method\": \"SUBSCRIBE\",";
        json << "\"params\": [\"" << stream_name << "\"],";
        json << "\"id\": " << std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        json << "}";
        
        std::string subscription_message = json.str();
        std::cout << "Sending individual subscription: " << subscription_message << std::endl;
        
        // Send the subscription message (unlock before calling send_message to avoid deadlock)
        auto self = shared_from_this();
        ioc_.post([self, subscription_message]() {
            self->send_message(subscription_message);
        });
    }
}

void Connection::unsubscribe_stream(const std::string& stream_name) {
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        stream_callbacks_.erase(stream_name);
    }

    // If already connected, send individual unsubscription message
    if (is_connected_) {
        std::ostringstream json;
        json << "{";
        json << "\"method\": \"UNSUBSCRIBE\",";
        json << "\"params\": [\"" << stream_name << "\"],";
        json << "\"id\": " << std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        json << "}";
        
        std::string unsubscription_message = json.str();
        std::cout << "Sending unsubscription: " << unsubscription_message << std::endl;
        
        auto self = shared_from_this();
        ioc_.post([self, unsubscription_message]() {
            self->send_message(unsubscription_message);
        });
    }
}

bool Connection::send_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!is_connected_ || !ws_) {
        return false;
    }

    try {
        ws_->write(net::buffer(message));
        return true;
    } catch (const std::exception& e) {
        handle_connection_error("Failed to send message: " + std::string(e.what()));
        return false;
    }
}

void Connection::set_connection_callbacks(ConnectionCallback on_connected, ConnectionCallback on_disconnected) {
    on_connected_ = std::move(on_connected);
    on_disconnected_ = std::move(on_disconnected);
}

void Connection::set_error_callback(ErrorCallback callback) {
    error_callback_ = std::move(callback);
}

bool Connection::is_connected() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return is_connected_ && ws_ && ws_->is_open();
}

void Connection::set_auto_reconnect(bool enable) {
    auto_reconnect_ = enable;
}

std::vector<std::string> Connection::get_subscribed_streams() const {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    std::vector<std::string> streams;
    streams.reserve(stream_callbacks_.size());
    
    for (const auto& pair : stream_callbacks_) {
        streams.push_back(pair.first);
    }
    
    return streams;
}

void Connection::start() {
    should_stop_ = false;
    if (!is_connected()) {
        connect();
    }
}

void Connection::stop() {
    should_stop_ = true;
    auto_reconnect_ = false;
    disconnect();
}

void Connection::do_connect() {
    auto self = shared_from_this();
    
    // Resolve hostname
    auto resolver = std::make_shared<tcp::resolver>(ioc_);
    resolver->async_resolve(
        config_.host,
        config_.port,
        [self, resolver](beast::error_code ec, tcp::resolver::results_type results) {
            if (ec) {
                self->handle_connection_error("DNS resolution failed: " + ec.message());
                return;
            }

            // Connect TCP
            net::async_connect(
                self->ws_->next_layer().next_layer(),
                results.begin(),
                results.end(),
                [self](beast::error_code ec, tcp::resolver::results_type::iterator endpoint_it) {
                    if (ec) {
                        self->handle_connection_error("TCP connection failed: " + ec.message());
                        return;
                    }

                    // Proceed to SSL handshake
                    self->ws_->next_layer().async_handshake(
                        ssl::stream_base::client,
                        [self](beast::error_code ec) {
                            if (ec) {
                                self->handle_connection_error("SSL handshake failed: " + ec.message());
                                return;
                            }

                            self->do_handshake();
                        }
                    );
                }
            );
        }
    );
}

void Connection::do_handshake() {
    auto self = shared_from_this();
    
    // Set WebSocket options
    ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_->set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(beast::http::field::user_agent, "grab/1.0");
        }
    ));

    // Set ping callback to handle incoming pings
    ws_->control_callback([self](websocket::frame_type kind, beast::string_view payload) {
        if (kind == websocket::frame_type::ping) {
            websocket::ping_data ping_data;
            std::memcpy(ping_data.data(), payload.data(), std::min(payload.size(), ping_data.size()));
            self->handle_ping(ping_data);
        }
    });

    // Connect to base WebSocket endpoint (not combined streams)
    std::string target = config_.endpoint;
    
    // Debug output to see what URL is being used
    std::cout << "WebSocket connecting to: " << target << std::endl;
    std::cout << "Registered streams count: " << stream_callbacks_.size() << std::endl;
    
    // Perform WebSocket handshake
    ws_->async_handshake(
        config_.host,
        target,
        [self](beast::error_code ec) {
            if (ec) {
                self->handle_connection_error("WebSocket handshake failed: " + ec.message());
                return;
            }

            {
                std::lock_guard<std::mutex> lock(self->state_mutex_);
                self->is_connected_ = true;
                self->is_connecting_ = false;
                self->connection_start_time_ = std::chrono::steady_clock::now();
            }

            // Start timers and reading
            self->start_connection_timer();
            self->reset_ping_timer();
            self->do_read();

            // Send subscription messages for all registered streams
            self->send_subscription_messages();

            if (self->on_connected_) {
                self->on_connected_();
            }
        }
    );
}

void Connection::do_read() {
    if (!is_connected() || should_stop_) {
        return;
    }

    auto self = shared_from_this();
    buffer_.clear();

    ws_->async_read(
        buffer_,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            self->on_read(ec, bytes_transferred);
        }
    );
}

void Connection::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (should_stop_) {
        return;
    }

    if (ec) {
        handle_connection_error("Read error: " + ec.message());
        return;
    }

    try {
        // Reset ping timer since we received data
        reset_ping_timer();

        // Convert buffer to string and process
        std::string message = beast::buffers_to_string(buffer_.data());
        process_message(message);

        // Continue reading
        do_read();

    } catch (const std::exception& e) {
        handle_connection_error("Error processing message: " + std::string(e.what()));
    }
}

void Connection::handle_ping(const websocket::ping_data& frame) {
    try {
        // Send pong response with the same payload
        if (ws_ && ws_->is_open()) {
            ws_->pong(frame);
            last_ping_time_ = std::chrono::steady_clock::now();
        }
    } catch (const std::exception& e) {
        handle_connection_error("Failed to send pong: " + std::string(e.what()));
    }
}

void Connection::process_message(const std::string& message) {
    try {
        std::cout << "Received message: " << message << std::endl;
        // Simple JSON parsing for combined stream format
        // Look for pattern: {"stream":"streamname","data":{...}}
        std::regex combined_pattern("\"stream\"\\s*:\\s*\"([^\"]+)\"\\s*,\\s*\"data\"\\s*:\\s*(.+)");
        std::smatch matches;

        if (std::regex_search(message, matches, combined_pattern)) {
            // Combined stream format
            std::string stream_name = matches[1].str();
            
            // Extract the data part - find the start of data and extract the rest
            std::size_t data_start = message.find("\"data\":");
            if (data_start != std::string::npos) {
                data_start += 7; // Skip "data":
                
                // Find the JSON object for data (skip whitespace)
                while (data_start < message.length() && std::isspace(message[data_start])) {
                    data_start++;
                }
                
                // Extract from data start to the end, removing the closing brace
                std::string data_json = message.substr(data_start);
                if (!data_json.empty() && data_json.back() == '}') {
                    data_json.pop_back(); // Remove the last }
                }
                
                // Find callback for this stream
                std::lock_guard<std::mutex> lock(callbacks_mutex_);
                auto it = stream_callbacks_.find(stream_name);
                if (it != stream_callbacks_.end() && it->second) {
                    it->second(stream_name, data_json);
                }
            }
        } else {
            // Single stream format - use first registered callback or broadcast to all
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            if (!stream_callbacks_.empty()) {
                auto first_callback = stream_callbacks_.begin();
                first_callback->second(first_callback->first, message);
            }
        }

    } catch (const std::exception& e) {
        if (error_callback_) {
            error_callback_("Failed to parse message: " + std::string(e.what()) + " | Message: " + message);
        }
    }
}

void Connection::handle_connection_error(const std::string& error_message) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        is_connected_ = false;
        is_connecting_ = false;
    }

    if (error_callback_) {
        error_callback_(error_message);
    }

    if (on_disconnected_) {
        on_disconnected_();
    }

    if (auto_reconnect_ && !should_stop_) {
        schedule_reconnect();
    }
}

void Connection::schedule_reconnect() {
    if (should_stop_ || !auto_reconnect_) {
        return;
    }

    auto self = shared_from_this();
    reconnect_timer_->expires_after(config_.reconnect_delay);
    reconnect_timer_->async_wait([self](beast::error_code ec) {
        if (!ec && !self->should_stop_) {
            self->connect();
        }
    });
}

void Connection::start_connection_timer() {
    auto self = shared_from_this();
    connection_timer_->expires_after(config_.connection_timeout);
    connection_timer_->async_wait([self](beast::error_code ec) {
        if (!ec) {
            self->on_connection_timeout();
        }
    });
}

void Connection::reset_ping_timer() {
    auto self = shared_from_this();
    ping_timer_->expires_after(config_.ping_timeout);
    ping_timer_->async_wait([self](beast::error_code ec) {
        if (!ec) {
            self->on_ping_timeout();
        }
    });
}

void Connection::on_connection_timeout() {
    handle_connection_error("Connection timeout after 24 hours - reconnecting as per Binance requirements");
}

void Connection::on_ping_timeout() {
    handle_connection_error("Ping timeout - no activity detected within timeout period");
}

void Connection::send_subscription_messages() {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    
    if (stream_callbacks_.empty()) {
        std::cout << "No streams to subscribe to" << std::endl;
        return;
    }

    // Build subscription message according to Binance WebSocket API
    std::ostringstream json;
    json << "{";
    json << "\"method\": \"SUBSCRIBE\",";
    json << "\"params\": [";
    
    bool first = true;
    for (const auto& pair : stream_callbacks_) {
        if (!first) {
            json << ",";
        }
        json << "\"" << pair.first << "\"";
        first = false;
    }
    
    json << "],";
    json << "\"id\": 1";
    json << "}";
    
    std::string subscription_message = json.str();
    std::cout << "Sending subscription message: " << subscription_message << std::endl;
    
    // Send the subscription message
    if (!send_message(subscription_message)) {
        std::cerr << "Failed to send subscription message" << std::endl;
    }
}

std::string Connection::build_stream_url() const {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    
    if (stream_callbacks_.empty()) {
        return config_.endpoint;
    }

    std::ostringstream url;
    url << config_.endpoint << "?streams=";
    
    bool first = true;
    for (const auto& pair : stream_callbacks_) {
        if (!first) {
            url << "/";
        }
        url << pair.first;
        first = false;
    }
    
    return url.str();
}
