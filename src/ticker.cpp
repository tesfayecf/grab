// Include the header file for this class
#include "ticker.hpp"
// Include iostream for console output and error messages
#include <iostream>
// Include stdexcept for standard exception types
#include <stdexcept>

// Constructor: Initialize the ticker with connection and stream parameters
Ticker::Ticker(std::shared_ptr<Connection> connection, const std::string& ticker_symbol, const std::string& stream_type)
    : connection_(std::move(connection))  // Store the shared connection (transfer ownership)
    , ticker_symbol_(ticker_symbol)       // Store the cryptocurrency pair symbol
    , stream_type_(stream_type)           // Store the data stream type
    , is_subscribed_(false)               // Initially not subscribed to any stream
{
    // Validate that the connection pointer is not null
    if (!connection_)
    {
        throw std::invalid_argument("Connection cannot be null");
    }

    // Validate that the ticker symbol is not empty
    if (ticker_symbol_.empty())
    {
        throw std::invalid_argument("Ticker symbol cannot be empty");
    }

    // Validate that the stream type is not empty
    if (stream_type_.empty())
    {
        throw std::invalid_argument("Stream type cannot be empty");
    }

    // Build the full stream name (e.g., "btcusdt@depth")
    // This follows the Binance WebSocket API format: symbol@streamType
    stream_name_ = ticker_symbol_ + "@" + stream_type_;
}

// Destructor: Ensure clean shutdown by unsubscribing from the stream
Ticker::~Ticker()
{
    stop(); // This will unsubscribe and clean up the ticker
}

// Set the callback function for handling incoming ticker messages
void Ticker::set_message_callback(MessageCallback callback)
{
    // Store the user-provided callback function for later invocation
    // This callback will be called whenever this ticker receives data
    message_callback_ = std::move(callback);
}

// Set the callback function for handling ticker-specific errors
void Ticker::set_error_callback(ErrorCallback callback)
{
    // Store the user-provided error callback function for later invocation
    // This callback will be called when errors occur during ticker operations
    error_callback_ = std::move(callback);
}

// Start subscribing to the ticker's data stream
void Ticker::start()
{
    // Check if already subscribed - no need to subscribe again
    if (is_subscribed_)
    {
        return; // Already subscribed
    }

    try
    {
        // Register callback with the connection for this stream
        // Use weak_ptr to avoid circular references between Ticker and Connection
        connection_->subscribe_stream(stream_name_,
                                      [weak_self = std::weak_ptr<Ticker>(shared_from_this())](const std::string &stream_name, const std::string &data)
                                      {
                                          // Convert weak_ptr to shared_ptr to safely access the Ticker instance
                                          if (auto self = weak_self.lock())
                                          {
                                              // Forward the message to the ticker's internal handler
                                              self->on_message_received(stream_name, data);
                                          }
                                          // If weak_ptr.lock() fails, the Ticker has been destroyed and we ignore the message
                                      });

        // Mark as subscribed only after successful registration
        is_subscribed_ = true;
    }
    catch (const std::exception &e)
    {
        // Handle any errors during subscription process
        on_connection_error("Failed to subscribe to stream: " + std::string(e.what()));
    }
}

// Stop subscribing to the ticker's data stream
void Ticker::stop()
{
    // Check if already unsubscribed - no work needed
    if (!is_subscribed_)
    {
        return; // Already unsubscribed
    }

    try
    {
        // Unregister from the connection's stream subscription
        // This will send an unsubscription message to the server if connected
        connection_->unsubscribe_stream(stream_name_);

        // Mark as unsubscribed after successful unregistration
        is_subscribed_ = false;
    }
    catch (const std::exception &e)
    {
        // Handle any errors during unsubscription process
        if (error_callback_)
        {
            error_callback_("Error during unsubscribe: " + std::string(e.what()));
        }
        // Note: We don't call on_connection_error here because this is during shutdown
    }
}

// Get the ticker symbol for this instance
const std::string &Ticker::get_ticker_symbol() const
{
    // Return a const reference to the stored ticker symbol
    return ticker_symbol_;
}

// Get the complete stream name for this ticker
const std::string &Ticker::get_stream_name() const
{
    // Return a const reference to the constructed stream name (symbol@type)
    return stream_name_;
}

// Check if the ticker is currently subscribed and receiving data
bool Ticker::is_subscribed() const
{
    // Return true only if all conditions are met:
    // 1. Local subscription flag is true
    // 2. Connection object still exists
    // 3. Connection is actively connected to the server
    return is_subscribed_ && connection_ && connection_->is_connected();
}

// Internal method to handle messages received from the connection
void Ticker::on_message_received(const std::string &stream_name, const std::string &data)
{
    // Verify this message is for our stream (safety check)
    if (stream_name != stream_name_)
    {
        return; // Not for us - ignore this message
    }

    // Forward the message data to the user's callback if one is registered
    if (message_callback_)
    {
        message_callback_(data);
    }
    // If no message callback is set, the message is silently ignored
}

// Internal method to handle ticker-specific error conditions
void Ticker::on_connection_error(const std::string &error_message)
{
    // Mark as unsubscribed since an error occurred
    is_subscribed_ = false;

    // Notify the user's error callback if one is registered
    if (error_callback_)
    {
        error_callback_(error_message);
    }
    else
    {
        // Default error handling - log to stderr if no callback is set
        std::cerr << "Ticker [" << ticker_symbol_ << "] Error: " << error_message << std::endl;
    }
}
