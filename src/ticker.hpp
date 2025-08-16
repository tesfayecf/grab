#pragma once

#include "connection.hpp"
#include <string>
#include <memory>
#include <functional>

namespace grab
{
namespace ticker
{
    
    // Type alias for the message callback function that handles incoming ticker data
    // Parameter: message - the JSON message received for this ticker
    using MessageCallback = std::function<void(const std::string &message)>;

    // Type alias for the error callback function that handles ticker-specific errors
    // Parameter: error - description of the error that occurred
    using ErrorCallback = std::function<void(const std::string &error)>;

    /**
     * @brief A wrapper class that encapsulates a connection for a single ticker
     *
     * This class manages ticker-specific operations using a shared Connection instance,
     * handles ticker subscription, and provides callback mechanisms for received data.
     * It provides a higher-level interface for subscribing to individual cryptocurrency
     * data streams while sharing a single WebSocket connection among multiple tickers.
     */
    class Ticker : public std::enable_shared_from_this<Ticker>
    {
    private:
        // Shared pointer to the Connection instance used for WebSocket communication
        // Multiple tickers can share the same connection for efficiency
        std::shared_ptr<connection::Connection> connection_; // Connection instance

        // The cryptocurrency pair symbol (e.g., "btcusdt", "ethbtc")
        std::string ticker_symbol_; // The ticker symbol

        // The type of data stream (e.g., "depth", "trade", "kline_1m", "bookTicker")
        std::string stream_type_; // The stream type (depth, trade, etc.)

        // The complete stream identifier combining symbol and type (e.g., "btcusdt@depth")
        // This is the actual stream name used by the Binance WebSocket API
        std::string stream_name_; // Full stream name (symbol@type)

        // User-defined callback function for handling incoming ticker messages
        MessageCallback on_message_; // Callback for messages

        // User-defined callback function for handling ticker-specific errors
        ErrorCallback on_error_; // Callback for errors

        // Flag indicating whether this ticker is currently subscribed to its stream
        bool is_subscribed_; // Flag to track subscription state

    public:
        /**
         * @brief Constructor that takes a shared Connection instance
         * @param connection Shared pointer to an established Connection
         * @param ticker_symbol The symbol to subscribe to (e.g., "btcusdt")
         * @param stream_type The stream type (e.g., "depth", "trade", "kline_1m")
         *
         * Creates a new Ticker instance that will use the provided shared Connection
         * to subscribe to data for a specific cryptocurrency pair and stream type.
         * The ticker_symbol should be lowercase (e.g., "btcusdt", "ethbtc").
         * Common stream types include "depth", "trade", "kline_1m", "bookTicker".
         */
        Ticker(std::shared_ptr<connection::Connection> connection, const std::string &ticker_symbol, const std::string &stream_type = "depth");

        /**
         * @brief Destructor
         *
         * Automatically calls stop() to ensure the ticker is properly unsubscribed
         * from the data stream before the object is destroyed.
         */
        ~Ticker();

        // Delete copy constructor and assignment operator to prevent accidental copying
        // Ticker objects manage stream subscriptions and should not be copied
        Ticker(const Ticker &) = delete;
        Ticker &operator=(const Ticker &) = delete;

        // Allow move constructor and assignment operator for efficient transfer of ownership
        // Moving a Ticker transfers its subscription and state to the new instance
        Ticker(Ticker &&) = default;
        Ticker &operator=(Ticker &&) = default;

        /**
         * @brief Set the callback function for handling received messages
         * @param on_message Function to be called when a message is received
         *
         * Registers a callback function that will be invoked whenever this ticker
         * receives data from its subscribed stream. The callback receives the
         * JSON message data as a string parameter.
         */
        void set_message_callback(MessageCallback on_message);

        /**
         * @brief Set the callback function for handling errors
         * @param on_error Function to be called when an error occurs
         *
         * Registers a callback function that will be invoked when ticker-specific
         * errors occur, such as subscription failures or connection issues.
         * If no error callback is set, errors will be logged to stderr.
         */
        void set_error_callback(ErrorCallback on_error);

        /**
         * @brief Subscribe to the ticker data stream
         * This method registers the ticker with the connection
         *
         * Initiates the subscription process by registering this ticker's callback
         * with the shared Connection. If the connection is already established,
         * a subscription message will be sent immediately. Otherwise, the
         * subscription will be activated when the connection is established.
         */
        void subscribe();

        /**
         * @brief Unsubscribe to the ticker data stream
         * This method unregisters the ticker from the connection
         *
         * Stops the subscription by unregistering the ticker's callback from
         * the shared Connection. If the connection is active, an unsubscription
         * message will be sent to the server.
         */
        void unsubscribe();

        /**
         * @brief Get the ticker symbol
         * @return The ticker symbol this instance is handling
         *
         * Returns the cryptocurrency pair symbol (e.g., "btcusdt") that this
         * ticker is configured to monitor.
         */
        const std::string &get_ticker_symbol() const;

        /**
         * @brief Get the stream name (symbol + stream type)
         * @return The full stream name (e.g., "btcusdt@depth")
         *
         * Returns the complete stream identifier that combines the ticker symbol
         * and stream type, which is used by the Binance WebSocket API to
         * identify specific data streams.
         */
        const std::string &get_stream_name() const;

        /**
         * @brief Check if the ticker is currently subscribed
         * @return true if subscribed, false otherwise
         *
         * Returns the current subscription status. This checks both the local
         * subscription flag and the underlying connection status to ensure
         * the ticker is actively receiving data.
         */
        bool is_subscribed() const;

    private:
        /**
         * @brief Internal method to handle received messages from the connection
         * @param stream_name The name of the stream the message came from
         * @param data The message data
         *
         * This is the internal callback registered with the Connection that receives
         * all messages for this ticker's stream. It verifies the message is for the
         * correct stream and then forwards it to the user's message callback.
         */
        void handle_message_(const std::string &stream_name, const std::string &data);

        /**
         * @brief Internal method to handle connection errors
         * @param error_message The error message
         *
         * Handles ticker-specific error conditions by updating the subscription
         * status and notifying the user's error callback if one is registered.
         */
        void on_connection_error_(const std::string &error_message);
    };

} // namespace ticker
} // namespace grab