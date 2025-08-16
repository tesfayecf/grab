#include "connection.hpp"
#include <regex>
#include <sstream>
#include <iostream>

// Constructor: Initialize all member variables and set up required components
Connection::Connection(net::io_context& ioc, const Config& config)
    : ioc_(ioc)                    // Store reference to IO context for async operations
    , config_(config)              // Store configuration parameters
    , is_connected_(false)         // Initially not connected
    , is_connecting_(false)        // Initially not attempting connection
    , auto_reconnect_(true)        // Enable auto-reconnect by default
    , should_stop_(false)          // Not stopped initially
{
    // Initialize SSL context for secure connections
    if (config_.use_ssl)
    {
        // Create SSL context for TLS v1.2 client connections
        ssl_ctx_ = std::make_unique<ssl::context>(ssl::context::tlsv12_client);
        // Set default certificate verification paths for SSL validation
        ssl_ctx_->set_default_verify_paths();
        // Enable peer certificate verification for security
        ssl_ctx_->set_verify_mode(ssl::verify_peer);
    }

    // Initialize all timers with the provided IO context
    // These timers will be used for reconnection, connection timeout, and ping timeout
    reconnect_timer_ = std::make_unique<net::steady_timer>(ioc_);
    connection_timer_ = std::make_unique<net::steady_timer>(ioc_);
    ping_timer_ = std::make_unique<net::steady_timer>(ioc_);
}

// Destructor: Ensure clean shutdown by calling stop()
Connection::~Connection()
{
    stop(); // This will disconnect and clean up all resources
}

// Initiate connection to the WebSocket server
bool Connection::connect()
{
    // Lock the state mutex to ensure thread-safe access to connection state
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Check if already connected or in the process of connecting
    if (is_connected_ || is_connecting_)
    {
        return is_connected_; // Return current connection status
    }

    try
    {
        // Create new WebSocket stream based on SSL configuration
        if (config_.use_ssl)
        {
            // Create SSL-enabled WebSocket stream using the configured SSL context
            ws_ = std::make_unique<WebSocketStream>(ioc_, *ssl_ctx_);
        }
        else
        {
            // For non-SSL connections, we'd need a different stream type
            // For now, assuming SSL is always used for Binance (production requirement)
            throw std::runtime_error("Non-SSL connections not implemented");
        }

        // Set connection state to indicate connection attempt is in progress
        is_connecting_ = true;

        // Start the asynchronous connection process
        do_connect();

        // Return true to indicate connection initiation was successful
        // Note: This doesn't mean the connection is established yet (it's async)
        return true;
    }
    catch (const std::exception &e)
    {
        // Handle any exceptions during connection setup
        handle_connection_error("Failed to initiate connection: " + std::string(e.what()));

        // Return false to indicate connection initiation failed
        return false;
    }
}

// Disconnect from the WebSocket server gracefully
void Connection::disconnect()
{
    // Lock the state mutex to ensure thread-safe access to connection state
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Check if already disconnected - no work needed
    if (!is_connected_)
    {
        return;
    }

    try
    {
        // Cancel all active timers to prevent them from firing during shutdown
        reconnect_timer_->cancel();  // Stop reconnection attempts
        connection_timer_->cancel(); // Stop 24-hour timeout timer
        ping_timer_->cancel();       // Stop ping timeout monitoring

        // Close WebSocket connection gracefully if it's open
        if (ws_ && ws_->is_open())
        {
            // Send a normal close frame to the server before disconnecting
            ws_->close(websocket::close_code::normal);
        }

        // Update connection state flags
        is_connected_ = false;  // Mark as disconnected
        is_connecting_ = false; // Not attempting to connect

        // Notify user code that disconnection has occurred
        if (on_disconnected_)
        {
            on_disconnected_();
        }
    }
    catch (const std::exception &e)
    {
        // Handle any errors during disconnection process
        if (error_callback_)
        {
            error_callback_("Error during disconnect: " + std::string(e.what()));
        }
    }
}

// Subscribe to a specific data stream with a callback function
void Connection::subscribe_stream(const std::string &stream_name, MessageCallback callback)
{
    // Lock the callbacks mutex to ensure thread-safe access to the callbacks map
    std::lock_guard<std::mutex> lock(callbacks_mutex_);

    // Store the callback function for this stream name
    // This will overwrite any existing callback for the same stream
    stream_callbacks_[stream_name] = std::move(callback);

    // Log the subscription for debugging purposes
    std::cout << "Subscribed to stream: " << stream_name
              << " (total streams: " << stream_callbacks_.size() << ")" << std::endl;

    // If already connected to the server, send immediate subscription message
    if (is_connected_)
    {
        // Build a JSON subscription message according to Binance WebSocket API format
        std::ostringstream json;
        json << "{";                                        // Start JSON object
        json << "\"method\": \"SUBSCRIBE\",";               // Subscription method
        json << "\"params\": [\"" << stream_name << "\"],"; // Array with stream name

        // Generate unique ID using current timestamp in milliseconds
        json << "\"id\": " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        json << "}"; // End JSON object

        // Convert to string for sending
        std::string subscription_message = json.str();
        std::cout << "Sending individual subscription: " << subscription_message << std::endl;

        // Send the subscription message asynchronously to avoid blocking
        // Use weak_ptr pattern to avoid circular references and potential deadlocks
        auto self = shared_from_this();
        ioc_.post([self, subscription_message]()
                  { self->send_message(subscription_message); });
    }
}

// Unsubscribe from a specific data stream
void Connection::unsubscribe_stream(const std::string &stream_name)
{
    {
        // Lock the callbacks mutex to safely remove the callback
        std::lock_guard<std::mutex> lock(callbacks_mutex_);

        // Remove the callback function for this stream from the map
        // This prevents future messages from being routed to the callback
        stream_callbacks_.erase(stream_name);
    }

    // If currently connected to the server, send unsubscription message
    if (is_connected_)
    {
        // Build a JSON unsubscription message according to Binance WebSocket API format
        std::ostringstream json;
        json << "{";                                        // Start JSON object
        json << "\"method\": \"UNSUBSCRIBE\",";             // Unsubscription method
        json << "\"params\": [\"" << stream_name << "\"],"; // Array with stream name

        // Generate unique ID using current timestamp in milliseconds
        json << "\"id\": " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        json << "}"; // End JSON object

        // Convert to string for sending
        std::string unsubscription_message = json.str();
        std::cout << "Sending unsubscription: " << unsubscription_message << std::endl;

        // Send the unsubscription message asynchronously
        auto self = shared_from_this();
        ioc_.post([self, unsubscription_message]()
                  { self->send_message(unsubscription_message); });
    }
}

// Send a raw message through the WebSocket connection
bool Connection::send_message(const std::string &message)
{
    // Lock the state mutex to ensure thread-safe access to connection state
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Verify that we're connected and have a valid WebSocket stream
    if (!is_connected_ || !ws_)
    {
        return false; // Cannot send if not connected
    }

    try
    {
        // Send the message through the WebSocket using Boost.Asio buffer
        // This is a synchronous operation that will block until sent
        ws_->write(net::buffer(message));
        return true; // Successfully sent
    }
    catch (const std::exception &e)
    {
        // Handle any errors during message sending
        handle_connection_error("Failed to send message: " + std::string(e.what()));
        return false; // Failed to send
    }
}

// Set callback functions for connection state changes
void Connection::set_connection_callbacks(ConnectionCallback on_connected, ConnectionCallback on_disconnected)
{
    // Store the callback functions for later invocation
    on_connected_ = std::move(on_connected);       // Called when connection established
    on_disconnected_ = std::move(on_disconnected); // Called when connection lost
}

// Set callback function for error handling
void Connection::set_error_callback(ErrorCallback callback)
{
    // Store the error callback function for later invocation when errors occur
    error_callback_ = std::move(callback);
}

// Check if the connection is currently active
bool Connection::is_connected() const
{
    // Lock the state mutex for thread-safe access to connection state
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Return true only if all conditions are met:
    // 1. is_connected_ flag is true
    // 2. WebSocket stream object exists
    // 3. WebSocket stream is actually open at the protocol level
    return is_connected_ && ws_ && ws_->is_open();
}

// Enable or disable automatic reconnection
void Connection::set_auto_reconnect(bool enable)
{
    // Set the auto-reconnect flag - this controls whether the connection
    // will attempt to reconnect automatically when the connection is lost
    auto_reconnect_ = enable;
}

// Get a list of all currently subscribed stream names
std::vector<std::string> Connection::get_subscribed_streams() const
{
    // Lock the callbacks mutex for thread-safe access to the streams map
    std::lock_guard<std::mutex> lock(callbacks_mutex_);

    // Create a vector to hold the stream names
    std::vector<std::string> streams;

    // Reserve space for efficiency (avoid multiple reallocations)
    streams.reserve(stream_callbacks_.size());

    // Extract all stream names from the map keys
    for (const auto &pair : stream_callbacks_)
    {
        streams.push_back(pair.first); // pair.first is the stream name
    }

    // Return the list of stream names
    return streams;
}

void Connection::start()
{
    should_stop_ = false;
    if (!is_connected())
    {
        connect();
    }
}

void Connection::stop()
{
    should_stop_ = true;
    auto_reconnect_ = false;
    disconnect();
}

void Connection::do_connect()
{
    auto self = shared_from_this();

    // Resolve hostname
    auto resolver = std::make_shared<tcp::resolver>(ioc_);
    resolver->async_resolve(
        config_.host,
        config_.port,
        [self, resolver](beast::error_code ec, tcp::resolver::results_type results)
        {
            if (ec)
            {
                self->handle_connection_error("DNS resolution failed: " + ec.message());
                return;
            }

            // Connect TCP
            net::async_connect(
                self->ws_->next_layer().next_layer(),
                results.begin(),
                results.end(),
                [self](beast::error_code ec, tcp::resolver::results_type::iterator endpoint_it)
                {
                    if (ec)
                    {
                        self->handle_connection_error("TCP connection failed: " + ec.message());
                        return;
                    }

                    // Proceed to SSL handshake
                    self->ws_->next_layer().async_handshake(
                        ssl::stream_base::client,
                        [self](beast::error_code ec)
                        {
                            if (ec)
                            {
                                self->handle_connection_error("SSL handshake failed: " + ec.message());
                                return;
                            }

                            self->do_handshake();
                        });
                });
        });
}

void Connection::do_handshake()
{
    auto self = shared_from_this();

    // Set WebSocket options
    ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_->set_option(websocket::stream_base::decorator(
        [](websocket::request_type &req)
        {
            req.set(beast::http::field::user_agent, "grab/1.0");
        }));

    // Set ping callback to handle incoming pings
    ws_->control_callback([self](websocket::frame_type kind, beast::string_view payload)
                          {
        if (kind == websocket::frame_type::ping) {
            websocket::ping_data ping_data;
            std::memcpy(ping_data.data(), payload.data(), std::min(payload.size(), ping_data.size()));
            self->handle_ping(ping_data);
        } });

    // Connect to base WebSocket endpoint (not combined streams)
    std::string target = config_.endpoint;

    // Debug output to see what URL is being used
    std::cout << "WebSocket connecting to: " << target << std::endl;
    std::cout << "Registered streams count: " << stream_callbacks_.size() << std::endl;

    // Perform WebSocket handshake
    ws_->async_handshake(
        config_.host,
        target,
        [self](beast::error_code ec)
        {
            if (ec)
            {
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

            if (self->on_connected_)
            {
                self->on_connected_();
            }
        });
}

void Connection::do_read()
{
    if (!is_connected() || should_stop_)
    {
        return;
    }

    auto self = shared_from_this();
    buffer_.clear();

    ws_->async_read(
        buffer_,
        [self](beast::error_code ec, std::size_t bytes_transferred)
        {
            self->on_read(ec, bytes_transferred);
        });
}

void Connection::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
    if (should_stop_)
    {
        return;
    }

    if (ec)
    {
        handle_connection_error("Read error: " + ec.message());
        return;
    }

    try
    {
        // Reset ping timer since we received data
        reset_ping_timer();

        // Convert buffer to string and process
        std::string message = beast::buffers_to_string(buffer_.data());
        process_message(message);

        // Continue reading
        do_read();
    }
    catch (const std::exception &e)
    {
        handle_connection_error("Error processing message: " + std::string(e.what()));
    }
}

void Connection::handle_ping(const websocket::ping_data &frame)
{
    try
    {
        // Send pong response with the same payload
        if (ws_ && ws_->is_open())
        {
            ws_->pong(frame);
            last_ping_time_ = std::chrono::steady_clock::now();
        }
    }
    catch (const std::exception &e)
    {
        handle_connection_error("Failed to send pong: " + std::string(e.what()));
    }
}

void Connection::process_message(const std::string &message)
{
    try
    {
        std::cout << "Received message: " << message << std::endl;
        // Simple JSON parsing for combined stream format
        // Look for pattern: {"stream":"streamname","data":{...}}
        std::regex combined_pattern("\"stream\"\\s*:\\s*\"([^\"]+)\"\\s*,\\s*\"data\"\\s*:\\s*(.+)");
        std::smatch matches;

        if (std::regex_search(message, matches, combined_pattern))
        {
            // Combined stream format
            std::string stream_name = matches[1].str();

            // Extract the data part - find the start of data and extract the rest
            std::size_t data_start = message.find("\"data\":");
            if (data_start != std::string::npos)
            {
                data_start += 7; // Skip "data":

                // Find the JSON object for data (skip whitespace)
                while (data_start < message.length() && std::isspace(message[data_start]))
                {
                    data_start++;
                }

                // Extract from data start to the end, removing the closing brace
                std::string data_json = message.substr(data_start);
                if (!data_json.empty() && data_json.back() == '}')
                {
                    data_json.pop_back(); // Remove the last }
                }

                // Find callback for this stream
                std::lock_guard<std::mutex> lock(callbacks_mutex_);
                auto it = stream_callbacks_.find(stream_name);
                if (it != stream_callbacks_.end() && it->second)
                {
                    it->second(stream_name, data_json);
                }
            }
        }
        else
        {
            // Single stream format - use first registered callback or broadcast to all
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            if (!stream_callbacks_.empty())
            {
                auto first_callback = stream_callbacks_.begin();
                first_callback->second(first_callback->first, message);
            }
        }
    }
    catch (const std::exception &e)
    {
        if (error_callback_)
        {
            error_callback_("Failed to parse message: " + std::string(e.what()) + " | Message: " + message);
        }
    }
}

void Connection::handle_connection_error(const std::string &error_message)
{
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        is_connected_ = false;
        is_connecting_ = false;
    }

    if (error_callback_)
    {
        error_callback_(error_message);
    }

    if (on_disconnected_)
    {
        on_disconnected_();
    }

    if (auto_reconnect_ && !should_stop_)
    {
        schedule_reconnect();
    }
}

void Connection::schedule_reconnect()
{
    if (should_stop_ || !auto_reconnect_)
    {
        return;
    }

    auto self = shared_from_this();
    reconnect_timer_->expires_after(config_.reconnect_delay);
    reconnect_timer_->async_wait([self](beast::error_code ec)
                                 {
        if (!ec && !self->should_stop_) {
            self->connect();
        } });
}

void Connection::start_connection_timer()
{
    auto self = shared_from_this();
    connection_timer_->expires_after(config_.connection_timeout);
    connection_timer_->async_wait([self](beast::error_code ec)
                                  {
        if (!ec) {
            self->on_connection_timeout();
        } });
}

void Connection::reset_ping_timer()
{
    auto self = shared_from_this();
    ping_timer_->expires_after(config_.ping_timeout);
    ping_timer_->async_wait([self](beast::error_code ec)
                            {
        if (!ec) {
            self->on_ping_timeout();
        } });
}

void Connection::on_connection_timeout()
{
    handle_connection_error("Connection timeout after 24 hours - reconnecting as per Binance requirements");
}

void Connection::on_ping_timeout()
{
    handle_connection_error("Ping timeout - no activity detected within timeout period");
}

void Connection::send_subscription_messages()
{
    std::lock_guard<std::mutex> lock(callbacks_mutex_);

    if (stream_callbacks_.empty())
    {
        std::cout << "No streams to subscribe to" << std::endl;
        return;
    }

    // Build subscription message according to Binance WebSocket API
    std::ostringstream json;
    json << "{";
    json << "\"method\": \"SUBSCRIBE\",";
    json << "\"params\": [";

    bool first = true;
    for (const auto &pair : stream_callbacks_)
    {
        if (!first)
        {
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
    if (!send_message(subscription_message))
    {
        std::cerr << "Failed to send subscription message" << std::endl;
    }
}

std::string Connection::build_stream_url() const
{
    std::lock_guard<std::mutex> lock(callbacks_mutex_);

    if (stream_callbacks_.empty())
    {
        return config_.endpoint;
    }

    std::ostringstream url;
    url << config_.endpoint << "?streams=";

    bool first = true;
    for (const auto &pair : stream_callbacks_)
    {
        if (!first)
        {
            url << "/";
        }
        url << pair.first;
        first = false;
    }

    return url.str();
}
