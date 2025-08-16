// Ensure this header is only included once to prevent redefinition errors
#pragma once

#include <mutex>
#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <boost/beast/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/steady_timer.hpp>

// Create convenient namespace aliases to reduce verbosity in code
namespace beast = boost::beast;         // Beast library for HTTP/WebSocket
namespace websocket = beast::websocket; // WebSocket-specific functionality
namespace net = boost::asio;            // Asio library for async networking
namespace ssl = boost::asio::ssl;       // SSL/TLS support
using tcp = boost::asio::ip::tcp;       // TCP protocol for network communication

namespace grab
{
namespace connection
{
    // Type alias for the WebSocket stream - combines SSL with WebSocket over TCP
    // This creates a secure WebSocket connection suitable for production trading data
    using WebSocketStream = websocket::stream<ssl::stream<tcp::socket>>;

    // Type alias for message callback functions that handle incoming stream data
    // Parameters: stream_name (e.g., "btcusdt@depth"), message_data (JSON payload)
    using MessageCallback = std::function<void(const std::string &stream_name, const std::string &data)>;

    // Type alias for connection event callbacks (connect/disconnect notifications)
    // These callbacks allow external code to react to connection state changes
    using ConnectionCallback = std::function<void()>;

    // Type alias for error handling callbacks to process connection and protocol errors
    // Parameter: error message describing what went wrong
    using ErrorCallback = std::function<void(const std::string &error)>;

    /**
     * @brief Connection configuration structure
     *
     * This struct contains all the configuration parameters needed to establish
     * and maintain a WebSocket connection to Binance's streaming API.
     */
    struct Config
    {
        // Hostname of the Binance WebSocket server (e.g., "stream.binance.com")
        std::string host;

        // Port number for the WebSocket connection (typically "9443" for SSL)
        std::string port;

        // WebSocket endpoint path (e.g., "/ws/btcusdt@bookTicker" for single stream)
        std::string endpoint; // For combined streams

        // Flag indicating whether to use SSL/TLS encryption (should always be true for production)
        bool use_ssl;

        // Delay between reconnection attempts when connection is lost
        std::chrono::seconds reconnect_delay;

        // Maximum connection duration before forced reconnect (Binance requires 24h max)
        std::chrono::seconds connection_timeout; // 24 hours as per Binance docs

        // Timeout for ping/pong responses to detect dead connections
        std::chrono::seconds ping_timeout; // 1 minute as per Binance docs

        // Default constructor that initializes all configuration values
        // Sets up reasonable defaults for connecting to Binance WebSocket API
        Config()
            :host("data-stream.binance.com")   // Official Binance WebSocket server
            ,port("9443")                      // Standard SSL WebSocket port
            ,endpoint("/")                     // Example single-stream endpoint
            ,use_ssl(true)                     // Always use SSL for security
            ,reconnect_delay(5)                // Wait 5 seconds between reconnects
            ,connection_timeout(24 * 60 * 60)  // 24 hours in seconds
            ,ping_timeout(60)                  // 1 minute ping timeout
        {
        }
    };

    /**
     * @brief A class that wraps WebSocket connection with automatic ping/pong handling and reconnection
     *
     * This class manages the WebSocket connection lifecycle, handles Binance's ping/pong requirements,
     * provides reconnection logic, and allows multiple tickers to register callbacks for specific symbols.
     * It implements the Binance WebSocket Stream API for real-time cryptocurrency data streaming.
     */
    class Connection : public std::enable_shared_from_this<Connection>
    {
    private:
        /**
         * Core components 
         * Fundamental objects needed for WebSocket operations
         */

        // Reference to the IO context that manages all async operations
        // Must remain valid for the lifetime of this Connection
        net::io_context &ioc_;

        // Configuration object containing connection parameters and timeouts
        Config config_;

        /**
         * SSL context for secure connections 
         * Manages TLS certificates and verification
         */
        std::unique_ptr<ssl::context> ssl_ctx_;

        // The actual WebSocket stream that handles the protocol communication
        std::unique_ptr<WebSocketStream> ws_;

        /**
         * Buffer for receiving incoming messages 
         * Managed by Boost.Beast
         */
        beast::flat_buffer buffer_;

        /**
         * Connection state 
         * Variables that track the current connection status
         */

        // Mutex protecting connection state variables for thread safety
        mutable std::mutex state_mutex_;

        // Flag indicating if the WebSocket connection is currently established
        bool is_connected_;

        // Flag indicating if a connection attempt is currently in progress
        bool is_connecting_;

        // Flag controlling whether to automatically reconnect on connection loss
        bool auto_reconnect_;

        // Flag indicating if the connection has been explicitly stopped
        bool should_stop_;

        /**
         * Stream management 
         * Handles subscription callbacks and stream routing
         */

        // Map of stream names to their associated callback functions
        // Key: stream name (e.g., "btcusdt@depth"), Value: callback function
        std::unordered_map<std::string, MessageCallback> stream_callbacks_;

        // Mutex protecting the stream callbacks map for thread safety
        mutable std::mutex callbacks_mutex_;

        /**
         * Timers 
         * Manage various timeout and periodic operations
         */

        // Timer for scheduling reconnection attempts after connection loss
        std::unique_ptr<net::steady_timer> reconnect_timer_;

        // Timer for enforcing 24-hour connection duration limit (Binance requirement)
        std::unique_ptr<net::steady_timer> connection_timer_;

        // Timer for detecting inactive connections (no ping/pong activity)
        std::unique_ptr<net::steady_timer> ping_timer_;

        /**
         * Callbacks 
         * User-defined functions for handling connection events
         */

        // Callback invoked when connection is successfully established
        ConnectionCallback on_connected_;

        // Callback invoked when connection is lost or disconnected
        ConnectionCallback on_disconnected_;

        // Callback invoked when errors occur during connection or data processing
        ErrorCallback on_error_;

        /**
         * Connection tracking 
         * Timestamps for monitoring connection health
         */

        // Timestamp of the last received ping frame (for connection health monitoring)
        std::chrono::steady_clock::time_point last_ping_time_;

        // Timestamp when the current connection was established (for 24h timeout)
        std::chrono::steady_clock::time_point connection_start_time_;

    public:
        /**
         * @brief Constructor
         * @param ioc IO context for async operations - manages all async networking operations
         * @param config Connection configuration with server details and timeouts
         *
         * Initializes the Connection with the provided IO context and configuration.
         * The IO context must remain alive for the entire lifetime of this Connection.
         */
        Connection(net::io_context &ioc, const Config &config = Config{});

        /**
         * @brief Destructor
         *
         * Automatically calls stop() to clean up any active connections and timers.
         * Ensures graceful shutdown of all networking operations.
         */
        ~Connection();

        // Delete copy constructor and assignment operator to prevent accidental copying
        // Connection objects manage network resources and should not be copied
        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;

        // Allow move constructor and assignment operator for efficient transfer of ownership
        // Moving a Connection transfers all its network resources to the new instance
        Connection(Connection &&) = default;
        Connection &operator=(Connection &&) = default;

        /**
         * @brief Set callback for connection events
         * @param on_connected Callback when connection is established
         * @param on_disconnected Callback when connection is lost
         *
         * Registers callbacks to be notified of connection state changes.
         * The on_connected callback is called after successful WebSocket handshake.
         * The on_disconnected callback is called when connection is lost or closed.
         */
        void set_connection_callbacks(ConnectionCallback on_connected, ConnectionCallback on_disconnected);

        /**
         * @brief Set error callback
         * @param on_error Callback for error events
         *
         * Registers a callback to handle connection errors, protocol errors,
         * and other exceptional conditions. Essential for robust error handling
         * in production applications.
         */
        void set_error_callback(ErrorCallback on_error);

        /**
         * @brief Connect to the WebSocket server
         * @return true if connection initiation successful, false otherwise
         *
         * Initiates the connection process including DNS resolution, TCP connection,
         * SSL handshake, and WebSocket handshake. This is an async operation that
         * returns immediately - use connection callbacks to detect when connected.
         */
        bool connect();

        /**
         * @brief Disconnect from the WebSocket server
         *
         * Gracefully closes the WebSocket connection, cancels all timers,
         * and triggers the disconnection callback. Safe to call multiple times.
         */
        void disconnect();

        /**
         * @brief Start the connection and begin processing
         * This method starts the async operations
         *
         * Initiates the connection process and sets the internal state to allow
         * connection attempts. Sets should_stop_ to false and calls connect()
         * if not already connected. This is the main entry point for starting
         * the connection lifecycle.
         */
        void start();

        /**
         * @brief Stop all operations and disconnect
         *
         * Stops the connection, sets should_stop_ flag to prevent reconnection,
         * disables auto-reconnect, and calls disconnect(). Use this for
         * graceful shutdown of the connection.
         */
        void stop();

        /**
         * @brief Send a raw message to the WebSocket
         * @param message The message to send (typically JSON)
         * @return true if sent successfully, false otherwise
         *
         * Sends a raw message through the WebSocket connection. Used internally
         * for subscription messages and can be used for custom protocol messages.
         * Thread-safe and handles connection state validation.
         */
        bool send_message(const std::string &message);

        /**
         * @brief Subscribe to a stream
         * @param stream_name The stream name to subscribe to (e.g., "btcusdt@depth")
         * @param callback Callback function for messages from this stream
         *
         * Registers a callback for a specific data stream. If already connected,
         * immediately sends a subscription message to the server. If not connected,
         * the subscription will be sent when connection is established.
         */
        void subscribe_stream(std::string id, const std::string &stream_name, MessageCallback callback);

        /**
         * @brief Unsubscribe from a stream
         * @param stream_name The stream name to unsubscribe from
         *
         * Removes the callback for a stream and sends an unsubscription message
         * to the server if currently connected. The callback will no longer
         * receive messages for this stream.
         */
        void unsubscribe_stream(std::string id, const std::string &stream_name);

        /**
         * @brief Check if currently connected
         * @return true if connected and WebSocket is open, false otherwise
         *
         * Thread-safe method to check the current connection status.
         * Returns true only if the WebSocket connection is active and ready
         * for sending/receiving messages.
         */
        bool is_connected() const;

        /**
         * @brief Get the list of currently subscribed streams
         * @return Vector of subscribed stream names
         *
         * Returns a copy of all currently subscribed stream names.
         * Useful for debugging and monitoring which streams are active.
         * Thread-safe operation.
         */
        std::vector<std::string> get_subscribed_streams() const;

    private:
        /**
         * @brief Internal method to establish TCP and SSL connection
         *
         * Performs DNS resolution of the hostname, establishes TCP connection,
         * and performs SSL handshake. This is the first phase of the connection
         * establishment process, followed by WebSocket handshake.
         */
        void connect_();

        /**
         * @brief Start the reconnection timer
         *
         * Schedules an automatic reconnection attempt after the configured
         * reconnect delay. Only activates if auto-reconnect is enabled and
         * the connection hasn't been explicitly stopped.
         */
        void reconnect_();

        /**
         * @brief Internal method to perform WebSocket handshake
         *
         * Configures WebSocket options (timeouts, user agent), sets up ping/pong
         * handling, and performs the WebSocket protocol handshake with the server.
         * After successful handshake, initiates subscription messages and starts
         * the read loop.
         */
        void handshake_();

        /**
         * @brief Internal method to handle async read operations
         *
         * Initiates an asynchronous read operation to receive the next message
         * from the WebSocket. This is part of the continuous read loop that
         * processes incoming messages throughout the connection lifetime.
         */
        void read_();

        /**
         * @brief Internal method to handle received messages
         * @param ec Error code from the async operation
         * @param bytes_transferred Number of bytes received
         *
         * Callback method invoked when an async read operation completes.
         * Processes the received data, handles any errors, resets ping timers,
         * and continues the read loop for ongoing message reception.
         */
        void on_data_(beast::error_code ec, std::size_t bytes_transferred);

        /**
         * @brief Process incoming message and route to appropriate callback
         * @param message The received message
         *
         * Parses incoming JSON messages, extracts stream names and data,
         * and routes the data to the appropriate registered callback function.
         * Handles both single-stream and combined-stream message formats.
         */
        void on_message(const std::string &message);

        /**
         * @brief Handle ping frames and send pong responses
         * @param frame The ping frame received
         *
         * Automatically responds to ping frames from the server with appropriate
         * pong responses. This is crucial for maintaining connection health as
         * required by the Binance WebSocket API specification.
         */
        void on_ping_(const websocket::ping_data &frame);

        /**
         * @brief Start the connection timeout timer (24 hours)
         *
         * Starts a timer that will force a reconnection after 24 hours as
         * required by Binance WebSocket API to prevent stale connections.
         * This ensures compliance with Binance's connection duration limits.
         */
        void start_connection_timer();

        /**
         * @brief Handle connection errors and attempt reconnection
         * @param error_message The error message
         *
         * Centralizes error handling by updating connection state, notifying
         * error callbacks, triggering disconnection callbacks, and initiating
         * reconnection if auto-reconnect is enabled.
         */
        void on_connection_error_(const std::string &error_message);

        /**
         * @brief Handle connection timeout (24 hours)
         *
         * Called when the 24-hour connection timeout expires. Forces a
         * reconnection to comply with Binance's requirement to refresh
         * connections periodically.
         */
        void on_connection_timeout();

        /**
         * @brief Handle ping timeout (no pong received)
         *
         * Called when no data has been received within the ping timeout period.
         * This indicates a potentially dead connection and triggers a reconnection
         * attempt to restore communication.
         */
        void on_ping_timeout();

        /**
         * @brief Reset the ping timeout timer
         *
         * Resets the ping timeout timer which monitors for connection health.
         * Called whenever data is received to indicate the connection is active.
         * If no data is received within the timeout period, the connection
         * is considered dead and will be reset.
         */
        void reset_ping_timer();

    };

} // namespace connection
} // namespace grab