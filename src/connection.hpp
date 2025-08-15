#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <functional>
#include <string>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

/**
 * @brief A class that wraps WebSocket connection with automatic ping/pong handling and reconnection
 * 
 * This class manages the WebSocket connection lifecycle, handles Binance's ping/pong requirements,
 * provides reconnection logic, and allows multiple tickers to register callbacks for specific symbols.
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    // Type alias for the WebSocket stream
    using WebSocketStream = websocket::stream<ssl::stream<tcp::socket>>;
    
    // Type alias for message callback (stream_name, message_data)
    using MessageCallback = std::function<void(const std::string& stream_name, const std::string& data)>;
    
    // Type alias for connection event callbacks
    using ConnectionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    /**
     * @brief Connection configuration structure
     */
    struct Config {
        std::string host;
        std::string port;
        std::string endpoint;  // For combined streams
        bool use_ssl;
        std::chrono::seconds reconnect_delay;
        std::chrono::seconds connection_timeout; // 24 hours as per Binance docs
        std::chrono::seconds ping_timeout; // 1 minute as per Binance docs
        
        // Default constructor
        Config() 
            : host("stream.binance.com")
            , port("9443")
            , endpoint("/ws/btcusdt@bookTicker")
            , use_ssl(true)
            , reconnect_delay(5)
            , connection_timeout(24 * 60 * 60)
            , ping_timeout(60)
        {}
    };

    /**
     * @brief Constructor
     * @param ioc IO context for async operations
     * @param config Connection configuration
     */
    Connection(net::io_context& ioc, const Config& config = Config{});

    /**
     * @brief Destructor
     */
    ~Connection();

    // Delete copy constructor and assignment operator
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Allow move constructor and assignment operator
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

    /**
     * @brief Connect to the WebSocket server
     * @return true if connection successful, false otherwise
     */
    bool connect();

    /**
     * @brief Disconnect from the WebSocket server
     */
    void disconnect();

    /**
     * @brief Subscribe to a stream
     * @param stream_name The stream name to subscribe to (e.g., "btcusdt@depth")
     * @param callback Callback function for messages from this stream
     */
    void subscribe_stream(const std::string& stream_name, MessageCallback callback);

    /**
     * @brief Unsubscribe from a stream
     * @param stream_name The stream name to unsubscribe from
     */
    void unsubscribe_stream(const std::string& stream_name);

    /**
     * @brief Send a raw message to the WebSocket
     * @param message The message to send
     * @return true if sent successfully, false otherwise
     */
    bool send_message(const std::string& message);

    /**
     * @brief Set callback for connection events
     * @param on_connected Callback when connection is established
     * @param on_disconnected Callback when connection is lost
     */
    void set_connection_callbacks(ConnectionCallback on_connected, ConnectionCallback on_disconnected);

    /**
     * @brief Set error callback
     * @param callback Callback for error events
     */
    void set_error_callback(ErrorCallback callback);

    /**
     * @brief Check if currently connected
     * @return true if connected, false otherwise
     */
    bool is_connected() const;

    /**
     * @brief Enable/disable automatic reconnection
     * @param enable true to enable auto-reconnect, false to disable
     */
    void set_auto_reconnect(bool enable);

    /**
     * @brief Get the list of currently subscribed streams
     * @return Vector of subscribed stream names
     */
    std::vector<std::string> get_subscribed_streams() const;

    /**
     * @brief Start the connection and begin processing
     * This method starts the async operations
     */
    void start();

    /**
     * @brief Stop all operations and disconnect
     */
    void stop();

private:
    /**
     * @brief Internal method to establish TCP and SSL connection
     */
    void do_connect();

    /**
     * @brief Internal method to perform WebSocket handshake
     */
    void do_handshake();

    /**
     * @brief Internal method to handle async read operations
     */
    void do_read();

    /**
     * @brief Internal method to handle received messages
     * @param ec Error code from the async operation
     * @param bytes_transferred Number of bytes received
     */
    void on_read(beast::error_code ec, std::size_t bytes_transferred);

    /**
     * @brief Handle ping frames and send pong responses
     * @param frame The ping frame received
     */
    void handle_ping(const websocket::ping_data& frame);

    /**
     * @brief Process incoming message and route to appropriate callback
     * @param message The received message
     */
    void process_message(const std::string& message);

    /**
     * @brief Handle connection errors and attempt reconnection
     * @param error_message The error message
     */
    void handle_connection_error(const std::string& error_message);

    /**
     * @brief Start the reconnection timer
     */
    void schedule_reconnect();

    /**
     * @brief Start the connection timeout timer (24 hours)
     */
    void start_connection_timer();

    /**
     * @brief Reset the ping timeout timer
     */
    void reset_ping_timer();

    /**
     * @brief Handle connection timeout (24 hours)
     */
    void on_connection_timeout();

    /**
     * @brief Handle ping timeout (no pong received)
     */
    void on_ping_timeout();

    /**
     * @brief Send subscription messages for all registered streams
     */
    void send_subscription_messages();

    /**
     * @brief Send subscription message for all registered streams
     */
    void resubscribe_all_streams();

    /**
     * @brief Build combined stream URL
     * @return The WebSocket target path with all subscribed streams
     */
    std::string build_stream_url() const;

private:
    // Core components
    net::io_context& ioc_;
    Config config_;
    std::unique_ptr<ssl::context> ssl_ctx_;
    std::unique_ptr<WebSocketStream> ws_;
    beast::flat_buffer buffer_;

    // Connection state
    mutable std::mutex state_mutex_;
    bool is_connected_;
    bool is_connecting_;
    bool auto_reconnect_;
    bool should_stop_;

    // Stream management
    std::unordered_map<std::string, MessageCallback> stream_callbacks_;
    mutable std::mutex callbacks_mutex_;

    // Timers
    std::unique_ptr<net::steady_timer> reconnect_timer_;
    std::unique_ptr<net::steady_timer> connection_timer_;
    std::unique_ptr<net::steady_timer> ping_timer_;

    // Callbacks
    ConnectionCallback on_connected_;
    ConnectionCallback on_disconnected_;
    ErrorCallback error_callback_;

    // Connection tracking
    std::chrono::steady_clock::time_point last_ping_time_;
    std::chrono::steady_clock::time_point connection_start_time_;
};
