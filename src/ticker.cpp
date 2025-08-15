#include "ticker.hpp"
#include <iostream>
#include <stdexcept>

Ticker::Ticker(std::shared_ptr<Connection> connection, const std::string& ticker_symbol, const std::string& stream_type)
    : connection_(std::move(connection))
    , ticker_symbol_(ticker_symbol)
    , stream_type_(stream_type)
    , is_subscribed_(false)
{
    if (!connection_) {
        throw std::invalid_argument("Connection cannot be null");
    }
    
    if (ticker_symbol_.empty()) {
        throw std::invalid_argument("Ticker symbol cannot be empty");
    }
    
    if (stream_type_.empty()) {
        throw std::invalid_argument("Stream type cannot be empty");
    }

    // Build the full stream name (e.g., "btcusdt@depth")
    stream_name_ = ticker_symbol_ + "@" + stream_type_;
}

Ticker::~Ticker() {
    stop();
}

void Ticker::set_message_callback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void Ticker::set_error_callback(ErrorCallback callback) {
    error_callback_ = std::move(callback);
}

void Ticker::start() {
    if (is_subscribed_) {
        return; // Already subscribed
    }

    try {
        // Register callback with the connection for this stream
        connection_->subscribe_stream(stream_name_, 
            [weak_self = std::weak_ptr<Ticker>(shared_from_this())](const std::string& stream_name, const std::string& data) {
                if (auto self = weak_self.lock()) {
                    self->on_message_received(stream_name, data);
                }
            });

        is_subscribed_ = true;

    } catch (const std::exception& e) {
        on_connection_error("Failed to subscribe to stream: " + std::string(e.what()));
    }
}

void Ticker::stop() {
    if (!is_subscribed_) {
        return; // Already unsubscribed
    }

    try {
        connection_->unsubscribe_stream(stream_name_);
        is_subscribed_ = false;
    } catch (const std::exception& e) {
        if (error_callback_) {
            error_callback_("Error during unsubscribe: " + std::string(e.what()));
        }
    }
}

const std::string& Ticker::get_ticker_symbol() const {
    return ticker_symbol_;
}

const std::string& Ticker::get_stream_name() const {
    return stream_name_;
}

bool Ticker::is_subscribed() const {
    return is_subscribed_ && connection_ && connection_->is_connected();
}

void Ticker::on_message_received(const std::string& stream_name, const std::string& data) {
    // Verify this message is for our stream
    if (stream_name != stream_name_) {
        return; // Not for us
    }

    // Call the message callback if set
    if (message_callback_) {
        message_callback_(data);
    }
}

void Ticker::on_connection_error(const std::string& error_message) {
    is_subscribed_ = false;

    if (error_callback_) {
        error_callback_(error_message);
    } else {
        // Default error handling - log to stderr
        std::cerr << "Ticker [" << ticker_symbol_ << "] Error: " << error_message << std::endl;
    }
}
