#pragma once

#include "connection.hpp"
#include <functional>
#include <string>
#include <memory>

/**
 * @brief A wrapper class that encapsulates a connection for a single ticker
 * 
 * This class manages ticker-specific operations using a shared Connection instance,
 * handles ticker subscription, and provides callback mechanisms for received data.
 */
class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    // Type alias for the message callback function
    using MessageCallback = std::function<void(const std::string& message)>;
    
    // Type alias for the error callback function
    using ErrorCallback = std::function<void(const std::string& error)>;

    /**
     * @brief Constructor that takes a shared Connection instance
     * @param connection Shared pointer to an established Connection
     * @param ticker_symbol The symbol to subscribe to (e.g., "btcusdt")
     * @param stream_type The stream type (e.g., "depth", "trade", "kline_1m")
     */
    Ticker(std::shared_ptr<Connection> connection, const std::string& ticker_symbol, const std::string& stream_type = "depth");

    /**
     * @brief Destructor
     */
    ~Ticker();

    // Delete copy constructor and assignment operator
    Ticker(const Ticker&) = delete;
    Ticker& operator=(const Ticker&) = delete;

    // Allow move constructor and assignment operator
    Ticker(Ticker&&) = default;
    Ticker& operator=(Ticker&&) = default;

    /**
     * @brief Set the callback function for handling received messages
     * @param callback Function to be called when a message is received
     */
    void set_message_callback(MessageCallback callback);

    /**
     * @brief Set the callback function for handling errors
     * @param callback Function to be called when an error occurs
     */
    void set_error_callback(ErrorCallback callback);

    /**
     * @brief Start subscribing to the ticker data stream
     * This method registers the ticker with the connection
     */
    void start();

    /**
     * @brief Stop subscribing to the ticker data stream
     * This method unregisters the ticker from the connection
     */
    void stop();

    /**
     * @brief Get the ticker symbol
     * @return The ticker symbol this instance is handling
     */
    const std::string& get_ticker_symbol() const;

    /**
     * @brief Get the stream name (symbol + stream type)
     * @return The full stream name (e.g., "btcusdt@depth")
     */
    const std::string& get_stream_name() const;

    /**
     * @brief Check if the ticker is currently subscribed
     * @return true if subscribed, false otherwise
     */
    bool is_subscribed() const;

private:
    /**
     * @brief Internal method to handle received messages from the connection
     * @param stream_name The name of the stream the message came from
     * @param data The message data
     */
    void on_message_received(const std::string& stream_name, const std::string& data);

    /**
     * @brief Internal method to handle connection errors
     * @param error_message The error message
     */
    void on_connection_error(const std::string& error_message);

private:
    std::shared_ptr<Connection> connection_;        // Connection instance
    std::string ticker_symbol_;                     // The ticker symbol
    std::string stream_type_;                       // The stream type (depth, trade, etc.)
    std::string stream_name_;                       // Full stream name (symbol@type)
    MessageCallback message_callback_;              // Callback for messages
    ErrorCallback error_callback_;                  // Callback for errors
    bool is_subscribed_;                            // Flag to track subscription state
};
