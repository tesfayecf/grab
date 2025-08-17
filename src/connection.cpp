#include "connection.hpp"
#include <regex>
#include <sstream>
#include <iostream>

namespace grab
{
namespace connection
{
    // public:

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
            this->ssl_ctx_ = std::make_unique<ssl::context>(ssl::context::tlsv12_client);
            // Set default certificate verification paths for SSL validation
            this->ssl_ctx_->set_default_verify_paths();
            // Enable peer certificate verification for security
            this->ssl_ctx_->set_verify_mode(ssl::verify_peer);
        }

        // Initialize all timers with the provided IO context
        // These timers will be used for reconnection, connection timeout, and ping timeout
        this->reconnect_timer_ = std::make_unique<net::steady_timer>(this->ioc_);
        this->connection_timer_ = std::make_unique<net::steady_timer>(this->ioc_);
        this->ping_timer_ = std::make_unique<net::steady_timer>(this->ioc_);
    }

    // Destructor: Ensure clean shutdown by calling stop()
    Connection::~Connection()
    {
        this->stop(); // This will disconnect and clean up all resources
    }

    // Set callback functions for connection state changes
    void Connection::set_connection_callbacks(ConnectionCallback on_connected, ConnectionCallback on_disconnected)
    {
        // Store the callback functions for later invocation
        this->on_connected_ = std::move(on_connected);       // Called when connection established
        this->on_disconnected_ = std::move(on_disconnected); // Called when connection lost
    }

    // Set callback function for error handling
    void Connection::set_error_callback(ErrorCallback on_error)
    {
        // Store the error callback function for later invocation when errors occur
        this->on_error_ = std::move(on_error);
    }

    // Initiate connection to the WebSocket server
    bool Connection::connect()
    {
        // Lock the state mutex to ensure thread-safe access to connection state
        std::lock_guard<std::mutex> lock(this->state_mutex_);

        // Check if already connected or in the process of connecting
        if (this->is_connected_ || this->is_connecting_)
        {
            return this->is_connected_; // Return current connection status
        }

        try
        {
            // Create new WebSocket stream based on SSL configuration
            if (this->config_.use_ssl)
            {
                // Create SSL-enabled WebSocket stream using the configured SSL context
                this->ws_ = std::make_unique<WebSocketStream>(this->ioc_, *this->ssl_ctx_);
            }
            else
            {
                // For non-SSL connections, we'd need a different stream type
                // For now, assuming SSL is always used for Binance (production requirement)
                throw std::runtime_error("Non-SSL connections not implemented");
            }

            // Set connection state to indicate connection attempt is in progress
            this->is_connecting_ = true;

            // Start the asynchronous connection process
            this->connect_();

            // Return true to indicate connection initiation was successful
            // Note: This doesn't mean the connection is established yet (it's async)
            return true;
        }
        catch (const std::exception &e)
        {
            // Handle any exceptions during connection setup
            this->on_connection_error_("Failed to initiate connection: " + std::string(e.what()));

            // Return false to indicate connection initiation failed
            return false;
        }
    }

    // Disconnect from the WebSocket server gracefully
    void Connection::disconnect()
    {
        // Lock the state mutex to ensure thread-safe access to connection state
        std::lock_guard<std::mutex> lock(this->state_mutex_);

        // Check if already disconnected - no work needed
        if (!this->is_connected_)
        {
            return;
        }

        try
        {
            // Cancel all active timers to prevent them from firing during shutdown
            this->reconnect_timer_->cancel();  // Stop reconnection attempts
            this->connection_timer_->cancel(); // Stop 24-hour timeout timer
            this->ping_timer_->cancel();       // Stop ping timeout monitoring

            // Close WebSocket connection gracefully if it's open
            if (this->ws_ && this->ws_->is_open())
            {
                // Send a normal close frame to the server before disconnecting
                this->ws_->close(websocket::close_code::normal);
            }

            // Update connection state flags
            this->is_connected_ = false;  // Mark as disconnected
            this->is_connecting_ = false; // Not attempting to connect

            // Notify user code that disconnection has occurred
            if (this->on_disconnected_)
            {
                this->on_disconnected_();
            }
        }
        catch (const std::exception &e)
        {
            // Handle any errors during disconnection process
            if (on_error_)
            {
                on_error_("Error during disconnect: " + std::string(e.what()));
            }
        }
    }

    // Start the connection service - public interface to begin operations
    void Connection::start()
    {
        // Clear the stop flag to allow the connection to operate normally
        should_stop_ = false;
        
        // Only attempt to connect if we're not already connected
        // This prevents redundant connection attempts
        if (!this->is_connected())
        {
            this->connect(); // Initiate the connection process
        }
    }

    // Stop the connection service - public interface to halt all operations
    void Connection::stop()
    {
        // Set the stop flag to signal that the connection should cease operations
        this->should_stop_ = true;
        
        // Disable auto-reconnect to prevent automatic reconnection attempts
        // This ensures a clean shutdown without unwanted reconnections
        this->auto_reconnect_ = false;
        
        // Disconnect from the server and clean up resources
        this->disconnect();
    }

    // Send a raw message through the WebSocket connection
    bool Connection::send_message(const std::string &message)
    {
        // Lock the state mutex to ensure thread-safe access to connection state
        std::lock_guard<std::mutex> lock(this->state_mutex_);

        // Verify that we're connected and have a valid WebSocket stream
        if (!this->is_connected_ || !this->ws_)
        {
            return false; // Cannot send if not connected
        }

        try
        {
            std::cout << "Sending message: " << message << std::endl;
            // Send the message through the WebSocket using Boost.Asio buffer
            // This is a synchronous operation that will block until sent
            this->ws_->write(net::buffer(message));
            return true; // Successfully sent
        }
        catch (const std::exception &e)
        {
            // Handle any errors during message sending
            this->on_connection_error_("Failed to send message: " + std::string(e.what()));
            return false; // Failed to send
        }
    }

    // Subscribe to a specific data stream with a callback function
    void Connection::subscribe_stream(std::string id, const std::string &stream_name, MessageCallback callback)
    {
        // Lock the callbacks mutex to ensure thread-safe access to the callbacks map
        std::lock_guard<std::mutex> lock(this->callbacks_mutex_);

        // Store the callback function for this stream name
        // This will overwrite any existing callback for the same stream
        this->stream_callbacks_[id] = std::move(callback);

        // Log the subscription for debugging purposes
        std::cout << "Subscribed to stream: " << stream_name << " (total streams: " << stream_callbacks_.size() << ")" << std::endl;

        // If already connected to the server, send immediate subscription message
        if (this->is_connected_)
        {
            // Build a JSON subscription message according to Binance WebSocket API format
            std::ostringstream json;
            json << "{";                                        // Start JSON object
            json << "\"method\": \"SUBSCRIBE\",";               // Subscription method
            json << "\"params\": [\"" << stream_name << "\"],"; // Array with stream name

            // Generate unique ID using current timestamp in milliseconds
            json << "\"id\": " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            json << "}"; // End JSON object

            // Convert to string for sending
            std::string subscription_message = json.str();
            std::cout << "Sending subscription: " << subscription_message << std::endl;

            // Send the subscription message asynchronously to avoid blocking
            // Use weak_ptr pattern to avoid circular references and potential deadlocks
            auto self = this->shared_from_this();
            this->ioc_.post(
                [self, subscription_message]()
                {
                    self->send_message(subscription_message);
                }
            );
        }
    }

    // Unsubscribe from a specific data stream
    void Connection::unsubscribe_stream(std::string id, const std::string &stream_name)
    {
        {
            // Lock the callbacks mutex to safely remove the callback
            std::lock_guard<std::mutex> lock(this->callbacks_mutex_);

            // Remove the callback function for this stream from the map
            // This prevents future messages from being routed to the callback
            this->stream_callbacks_.erase(stream_name);
        }

        // If currently connected to the server, send unsubscription message
        if (this->is_connected_)
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
            ioc_.post(
                [self, unsubscription_message]()
                {
                    self->send_message(unsubscription_message);
                }
            );
        }
    }

    // Check if the connection is currently active
    bool Connection::is_connected() const
    {
        // Lock the state mutex for thread-safe access to connection state
        std::lock_guard<std::mutex> lock(this->state_mutex_);

        // Return true only if all conditions are met:
        // 1. is_connected_ flag is true
        // 2. WebSocket stream object exists
        // 3. WebSocket stream is actually open at the protocol level
        return this->is_connected_ && this->ws_ && this->ws_->is_open();
    }

    // Get a list of all currently subscribed stream names
    std::vector<std::string> Connection::get_subscribed_streams() const
    {
        // Lock the callbacks mutex for thread-safe access to the streams map
        std::lock_guard<std::mutex> lock(this->callbacks_mutex_);

        // Create a vector to hold the stream names
        std::vector<std::string> streams;

        // Reserve space for efficiency (avoid multiple reallocations)
        streams.reserve(this->stream_callbacks_.size());

        // Extract all stream names from the map keys
        for (const auto &pair : this->stream_callbacks_)
        {
            streams.push_back(pair.first); // pair.first is the stream name
        }

        // Return the list of stream names
        return streams;
    }

    // private:

    // Begin the asynchronous connection process to the WebSocket server
    void Connection::connect_()
    {
        // Create a shared pointer to this object to ensure it stays alive
        // during the entire asynchronous connection process
        auto self = this->shared_from_this();

        // Resolve hostname to IP address using async DNS resolution
        // Create a TCP resolver on the heap to keep it alive during async operations
        auto resolver = std::make_shared<tcp::resolver>(this->ioc_);
        
        // Asynchronously resolve the hostname to IP addresses
        resolver->async_resolve(
            this->config_.host, // Target hostname (e.g., "stream.binance.com")
            this->config_.port, // Target port (e.g., "443" for HTTPS/WSS)
            
            // Lambda callback executed when DNS resolution completes
            [self, resolver](beast::error_code ec, tcp::resolver::results_type results)
            {
                // Check if DNS resolution failed
                if (ec)
                {
                    // Report the DNS resolution error and abort connection
                    self->on_connection_error_("DNS resolution failed: " + ec.message());
                    return;
                }

                // Proceed to TCP connection phase using resolved addresses
                // Connect to the TCP socket (lowest layer of the SSL WebSocket stack)
                net::async_connect(
                    self->ws_->next_layer().next_layer(), // Get the raw TCP socket
                    results.begin(),    // Start of resolved address list
                    results.end(),      // End of resolved address list
                    
                    // Lambda callback executed when TCP connection completes
                    [self](beast::error_code ec, tcp::resolver::results_type::iterator endpoint_it)
                    {
                        // Check if TCP connection failed
                        if (ec)
                        {
                            // Report the TCP connection error and abort
                            self->on_connection_error_("TCP connection failed: " + ec.message());
                            return;
                        }

                        // Proceed to SSL/TLS handshake phase
                        // Perform SSL handshake on top of the established TCP connection
                        self->ws_->next_layer().async_handshake(
                            ssl::stream_base::client, // We are the SSL client
                            
                            // Lambda callback executed when SSL handshake completes
                            [self](beast::error_code ec)
                            {
                                // Check if SSL handshake failed
                                if (ec)
                                {
                                    // Report the SSL handshake error and abort
                                    self->on_connection_error_("SSL handshake failed: " + ec.message());
                                    return;
                                }

                                // SSL connection established successfully
                                // Proceed to WebSocket handshake phase
                                self->handshake_();
                            }
                        );
                    }
                );
            }
        );
    }

    // Schedule an automatic reconnection attempt after a delay
    void Connection::reconnect_()
    {
        // Only schedule reconnection if conditions are appropriate
        if (this->should_stop_ || !this->auto_reconnect_)
        {
            return; // Exit if shutting down or auto-reconnect is disabled
        }

        // Create a shared pointer to this object for the async operation
        auto self = this->shared_from_this();
        
        // Set the timer to expire after the configured reconnection delay
        // This delay prevents rapid reconnection attempts that could overwhelm the server
        reconnect_timer_->expires_after(this->config_.reconnect_delay);
        
        // Start the asynchronous wait for the timer to expire
        reconnect_timer_->async_wait(
            [self](beast::error_code ec)
            {
                // Check if the timer completed successfully (not cancelled)
                if (!ec && !self->should_stop_) {
                    // Attempt to reconnect to the server
                    self->connect();
                }
            }
        );
    }

    // Perform the WebSocket handshake after SSL connection is established
    void Connection::handshake_()
    {
        // Create a shared pointer to this object for async operation lifetime management
        auto self = this->shared_from_this();

        // Configure WebSocket stream options for optimal operation
        // Set timeout options based on client role (recommended by Beast library)
        this->ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
        
        // Set custom HTTP headers for the WebSocket upgrade request
        this->ws_->set_option(websocket::stream_base::decorator(
            [](websocket::request_type &req)
            {
                // Set User-Agent header to identify our application
                req.set(beast::http::field::user_agent, "grab/1.0");
            }
        ));

        // Set up control frame callback to handle WebSocket ping/pong frames
        // This is essential for connection keep-alive and health monitoring
        this->ws_->control_callback(
            [self](websocket::frame_type kind, beast::string_view payload)
            {
                // Check if this is a ping frame from the server
                if (kind == websocket::frame_type::ping) {
                    // Create ping data structure to hold the payload
                    websocket::ping_data ping_data;
                    
                    // Copy the ping payload (limited by ping_data size)
                    std::memcpy(ping_data.data(), payload.data(), std::min(payload.size(), ping_data.size()));
                    
                    // Handle the ping by sending an appropriate pong response
                    self->on_ping_(ping_data);
                }
            }
        );

        // Determine the WebSocket endpoint path to connect to
        // Use the configured endpoint (typically starts with "/ws/")
        std::string target = this->config_.endpoint;

        // Debug output to help with connection troubleshooting
        std::cout << "WebSocket connecting to: " << target << std::endl;

        // Perform the actual WebSocket handshake with the server
        this->ws_->async_handshake(
            this->config_.host, // Host header value (e.g., "stream.binance.com")
            target,             // Request path (e.g., "/ws")
            
            // Lambda callback executed when WebSocket handshake completes
            [self](beast::error_code ec)
            {
                // Check if WebSocket handshake failed
                if (ec)
                {
                    // Report the handshake error and abort connection
                    self->on_connection_error_("WebSocket handshake failed: " + ec.message());
                    return;
                }

                // WebSocket connection successfully established!
                // Update connection state in a thread-safe manner
                {
                    std::lock_guard<std::mutex> lock(self->state_mutex_);
                    self->is_connected_ = true;    // Mark as fully connected
                    self->is_connecting_ = false;  // No longer in connecting state
                    
                    // Record when the connection was established for timeout tracking
                    self->connection_start_time_ = std::chrono::steady_clock::now();
                }

                // Start various timers for connection management
                self->start_connection_timer(); // 24-hour connection timeout (Binance requirement)
                self->reset_ping_timer();       // Monitor for server activity/ping timeout
                
                // Begin reading incoming messages from the server
                self->read_();

                // Notify the application that connection is now established
                if (self->on_connected_)
                {
                    self->on_connected_();
                }
            });
    }

    // Initiate asynchronous reading of messages from the WebSocket
    void Connection::read_()
    {
        // Check if we should stop reading (connection closing or stopped)
        if (!this->is_connected() || this->should_stop_)
        {
            return; // Exit early if connection is not active
        }

        // Create a shared pointer to this object for async operation lifetime management
        auto self = this->shared_from_this();
        
        // Clear any previous data from the read buffer
        // This ensures we start with a clean buffer for the next message
        this->buffer_.clear();

        // Begin asynchronous read operation from the WebSocket
        // This will read one complete WebSocket message (frame)
        this->ws_->async_read(
            this->buffer_, // Buffer to store the incoming message data
            
            // Lambda callback executed when read operation completes
            [self](beast::error_code ec, std::size_t bytes_transferred)
            {
                // Delegate to the read completion handler
                self->on_data_(ec, bytes_transferred);
            });
    }

    // Handle completion of an asynchronous read operation
    void Connection::on_data_(beast::error_code ec, std::size_t bytes_transferred)
    {
        // Check if we should stop processing (connection is being shut down)
        if (this->should_stop_)
        {
            return; // Exit early to avoid processing during shutdown
        }

        // Check if the read operation failed
        if (ec)
        {
            // Handle the read error by reporting it and potentially reconnecting
            this->on_connection_error_("Read error: " + ec.message());
            return;
        }

        try
        {
            // Reset ping timeout timer since we received data from the server
            // This indicates the connection is still active and healthy
            this->reset_ping_timer();

            // Convert the received buffer data to a string for processing
            // The buffer contains the complete WebSocket message
            std::string message = beast::buffers_to_string(this->buffer_.data());
            
            // Process the received message (parse and route to appropriate callbacks)
            this->on_message(message);

            // Continue reading the next message from the WebSocket
            // This maintains the continuous message reading loop
            this->read_();
        }
        catch (const std::exception &e)
        {
            // Handle any exceptions that occur during message processing
            this->on_connection_error_("Error processing message: " + std::string(e.what()));
        }
    }

    // Parse and route incoming WebSocket messages to the appropriate stream callbacks
    void Connection::on_message(const std::string &message)
    {
        try
        {
            // Call all the stream callbacks
            {
                std::lock_guard<std::mutex> lock(callbacks_mutex_);
                for (auto &callback : this->stream_callbacks_)
                {
                    callback.second(message);
                }
            }

            // // Parse messages using regex to handle Binance's combined stream format
            // // Binance combined streams use format: {"s":"streamname",{...}}
            // std::regex combined_pattern("\\s*\"s\"\\s*:\\s*\"([^\"]*)\"");
            // std::smatch matches;

            // // Check if this message matches the combined stream format
            // if (std::regex_search(message, matches, combined_pattern))
            // {
            //     // Extract the stream name from the first capture group
            //     std::string stream_name = matches[1].str();

            //     // Conver to lower case
            //     std::transform(stream_name.begin(), stream_name.end(), stream_name.begin(), ::tolower);

            //     // Thread-safe lookup of the callback for this specific stream
            //     std::lock_guard<std::mutex> lock(callbacks_mutex_);
            //     auto it = this->stream_callbacks_.find(stream_name);
                
            //     // If we found a callback for this stream, invoke it
            //     if (it != this->stream_callbacks_.end() && it->second)
            //     {
            //         // Call the registered callback with the stream name and data
            //         it->second(stream_name, message);
            //     }
            // }
            // else
            // {
            //     // Handle single stream format or messages that don't match combined format
            //     // This handles direct stream connections or non-standard message formats
            //     std::lock_guard<std::mutex> lock(callbacks_mutex_);
                
            //     // If we have any registered callbacks, use the first one
            //     // This is a fallback for single-stream connections
            //     if (!stream_callbacks_.empty())
            //     {
            //         auto first_callback = stream_callbacks_.begin();
            //         // Pass the entire message as-is to the callback
            //         first_callback->second(first_callback->first, message);
            //     }
            // }
        }
        catch (const std::exception &e)
        {
            // Handle any parsing or callback errors
            if (on_error_)
            {
                // Provide detailed error information including the problematic message
                on_error_("Failed to parse message: " + std::string(e.what()) + 
                            " | Message: " + message);
            }
        }
    }

    // Handle incoming ping frames from the server by sending pong responses
    void Connection::on_ping_(const websocket::ping_data &frame)
    {
        try
        {
            // Verify that we have a valid WebSocket connection before responding
            if (this->ws_ && this->ws_->is_open())
            {
                // Send a pong response with the same payload that was received in the ping
                // This is required by the WebSocket protocol to maintain connection health
                this->ws_->pong(frame);
                
                // Record the timestamp of this ping for monitoring purposes
                // This can be used to track server responsiveness and connection health
                last_ping_time_ = std::chrono::steady_clock::now();
            }
        }
        catch (const std::exception &e)
        {
            // Handle any errors that occur while sending the pong response
            // This could indicate connection problems or protocol issues
            this->on_connection_error_("Failed to send pong: " + std::string(e.what()));
        }
    }

    // Start the 24-hour connection timeout timer (Binance WebSocket requirement)
    void Connection::start_connection_timer()
    {
        // Create a shared pointer to this object for the async operation
        auto self = this->shared_from_this();
        
        // Set the timer to expire after the configured connection timeout
        // Binance requires WebSocket connections to be renewed every 24 hours
        this->connection_timer_->expires_after(config_.connection_timeout);
        
        // Start the asynchronous wait for the timer to expire
        this->connection_timer_->async_wait(
            [self](beast::error_code ec)
            {
                // Check if the timer completed successfully (not cancelled)
                if (!ec) {
                    // Handle the connection timeout by triggering a reconnection
                    self->on_connection_timeout();
                }
            }
        );
    }

    // Reset the ping timeout timer to monitor for server activity
    void Connection::reset_ping_timer()
    {
        // Create a shared pointer to this object for the async operation
        auto self = this->shared_from_this();
        
        // Set the timer to expire after the configured ping timeout period
        // This timer tracks whether we've received any data from the server recently
        this->ping_timer_->expires_after(config_.ping_timeout);
        
        // Start the asynchronous wait for the timer to expire
        this->ping_timer_->async_wait(
            [self](beast::error_code ec)
            {
                // Check if the timer completed successfully (not cancelled)
                if (!ec) {
                    // Handle the ping timeout (no server activity detected)
                    self->on_ping_timeout();
                }
            }
        );
    }

    // Handle connection errors and manage reconnection logic
    void Connection::on_connection_error_(const std::string &error_message)
    {
        // Update connection state in a thread-safe manner
        {
            std::lock_guard<std::mutex> lock(this->state_mutex_);
            
            // Mark the connection as disconnected and not in connecting state
            this->is_connected_ = false;  // No longer connected to the server
            this->is_connecting_ = false; // Not attempting to connect
        }

        // Notify the application about the error through the error callback
        if (this->on_error_)
        {
            this->on_error_(error_message);
        }

        // Notify the application that the connection has been lost
        if (this->on_disconnected_)
        {
            this->on_disconnected_();
        }

        // Attempt automatic reconnection if enabled and not shutting down
        if (this->auto_reconnect_ && !this->should_stop_)
        {
            // Schedule a reconnection attempt after the configured delay
            this->reconnect_();
        }
    }

    // Handle the 24-hour connection timeout event
    void Connection::on_connection_timeout()
    {
        // Trigger a reconnection due to the mandatory 24-hour timeout
        // Binance WebSocket API requires connections to be renewed every 24 hours
        // to ensure optimal performance and compliance with their service requirements
        this->on_connection_error_("Connection timeout after 24 hours - reconnecting as per Binance requirements");
    }

    // Handle ping timeout when no server activity is detected
    void Connection::on_ping_timeout()
    {
        // Trigger a reconnection due to lack of server communication
        // This indicates the connection may be stale or the server is unresponsive
        // Regular server activity (messages or pings) should reset this timer
        this->on_connection_error_("Ping timeout - no activity detected within timeout period");
    }

} // namespace connection
} // namespace grab