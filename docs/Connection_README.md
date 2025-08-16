# Connection Class

## Overview

The `Connection` class is a robust WebSocket client implementation designed specifically for connecting to Binance's cryptocurrency data streaming API. It provides automatic connection management, ping/pong handling, reconnection logic, and support for multiple concurrent data stream subscriptions.

## Key Features

- **Secure WebSocket Connection**: SSL/TLS encrypted connections to Binance streaming servers
- **Automatic Reconnection**: Configurable auto-reconnect with exponential backoff
- **Ping/Pong Handling**: Automatic response to server ping frames to maintain connection health
- **24-Hour Connection Cycling**: Complies with Binance's requirement to refresh connections every 24 hours
- **Multi-Stream Support**: Subscribe to multiple cryptocurrency data streams simultaneously
- **Thread-Safe Operations**: All public methods are thread-safe using mutex protection
- **Callback-Based Architecture**: Event-driven design with customizable callbacks for different events

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   User Code     │    │   Connection     │    │  Binance API    │
│                 │    │                  │    │                 │
│ ┌─────────────┐ │    │ ┌──────────────┐ │    │ ┌─────────────┐ │
│ │   Ticker    │ │◄──►│ │  WebSocket   │ │◄──►│ │   Stream    │ │
│ │   Ticker    │ │    │ │   Manager    │ │    │ │   Server    │ │
│ │   Ticker    │ │    │ │              │ │    │ │             │ │
│ └─────────────┘ │    │ └──────────────┘ │    │ └─────────────┘ │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

## Core Components

### Configuration (`Config` struct)
- **host**: Binance WebSocket server hostname (`stream.binance.com`)
- **port**: SSL port for WebSocket connections (`9443`)
- **endpoint**: WebSocket endpoint path (e.g., `/ws/btcusdt@bookTicker`)
- **use_ssl**: Enable SSL/TLS encryption (always `true` for production)
- **reconnect_delay**: Time between reconnection attempts (default: 5 seconds)
- **connection_timeout**: Maximum connection duration (24 hours per Binance requirement)
- **ping_timeout**: Maximum time without activity before considering connection dead (1 minute)

### Connection States
- **Disconnected**: Initial state, no active connection
- **Connecting**: DNS resolution, TCP connection, SSL handshake, WebSocket handshake in progress
- **Connected**: Active WebSocket connection ready for data streaming
- **Reconnecting**: Connection lost, attempting to reconnect

### Timer Management
- **Reconnection Timer**: Schedules automatic reconnection attempts
- **Connection Timer**: Enforces 24-hour connection limit
- **Ping Timer**: Monitors connection health and detects dead connections

## Usage Examples

### Basic Connection Setup

```cpp
#include "connection.hpp"
#include <boost/asio.hpp>

// Create IO context for async operations
boost::asio::io_context ioc;

// Create connection with default configuration
auto connection = std::make_shared<Connection>(ioc);

// Set up event callbacks
connection->set_connection_callbacks(
    []() { std::cout << "Connected!" << std::endl; },
    []() { std::cout << "Disconnected!" << std::endl; }
);

connection->set_error_callback(
    [](const std::string& error) {
        std::cerr << "Error: " << error << std::endl;
    }
);

// Start the connection
connection->start();

// Run the IO context (this will block and handle all async operations)
ioc.run();
```

### Custom Configuration

```cpp
// Create custom configuration
Connection::Config config;
config.host = "stream.binance.com";
config.port = "9443";
config.endpoint = "/ws/btcusdt@bookTicker";
config.reconnect_delay = std::chrono::seconds(10);  // 10 second reconnect delay

// Create connection with custom config
auto connection = std::make_shared<Connection>(ioc, config);
```

### Stream Subscription

```cpp
// Subscribe to a specific data stream
connection->subscribe_stream("btcusdt@depth", 
    [](const std::string& stream_name, const std::string& data) {
        std::cout << "Received data for " << stream_name << ": " << data << std::endl;
    });

// Subscribe to multiple streams
connection->subscribe_stream("ethusdt@trade", 
    [](const std::string& stream_name, const std::string& data) {
        // Handle ETH trade data
    });

connection->subscribe_stream("adausdt@kline_1m", 
    [](const std::string& stream_name, const std::string& data) {
        // Handle ADA kline data
    });
```

### Stream Management

```cpp
// Get list of subscribed streams
auto streams = connection->get_subscribed_streams();
for (const auto& stream : streams) {
    std::cout << "Subscribed to: " << stream << std::endl;
}

// Unsubscribe from a stream
connection->unsubscribe_stream("btcusdt@depth");

// Check connection status
if (connection->is_connected()) {
    std::cout << "Connection is active" << std::endl;
}

// Control auto-reconnect behavior
connection->set_auto_reconnect(false);  // Disable auto-reconnect
```

## Thread Safety

The Connection class is designed to be thread-safe:

- **State Protection**: All connection state variables are protected by `state_mutex_`
- **Callback Protection**: Stream callbacks map is protected by `callbacks_mutex_`
- **Async Operations**: All network operations are asynchronous and non-blocking
- **Safe Shutdown**: Proper cleanup and cancellation of all pending operations

## Error Handling

The class provides comprehensive error handling:

### Connection Errors
- DNS resolution failures
- TCP connection failures
- SSL handshake failures
- WebSocket handshake failures

### Runtime Errors
- Network timeouts
- Unexpected disconnections
- Protocol violations
- Message parsing errors

### Error Recovery
- Automatic reconnection with configurable delays
- Graceful degradation when connections fail
- Preservation of stream subscriptions across reconnections

## Binance API Compliance

The Connection class is specifically designed to comply with Binance WebSocket API requirements:

### Message Format Support
- **Subscription Messages**: JSON-formatted subscription requests
- **Unsubscription Messages**: JSON-formatted unsubscription requests
- **Combined Streams**: Support for multiple streams in a single connection
- **Response Handling**: Proper parsing of server responses and confirmations

### Connection Requirements
- **SSL/TLS**: All connections use encrypted transport
- **Ping/Pong**: Automatic response to server ping frames
- **24-Hour Limit**: Automatic reconnection every 24 hours
- **User Agent**: Proper identification in WebSocket headers

### Stream Naming
- Follows Binance convention: `symbol@streamType` (e.g., `btcusdt@depth`)
- Supports all Binance stream types: depth, trade, kline, bookTicker, etc.
- Case-sensitive stream names as required by Binance

## Performance Considerations

### Memory Management
- Uses smart pointers (`shared_ptr`, `unique_ptr`) for automatic resource management
- Efficient buffer management with `beast::flat_buffer`
- Minimal memory allocations in the message processing hot path

### Network Efficiency
- Persistent WebSocket connection reduces connection overhead
- Multiplexed streams share a single connection
- Asynchronous I/O prevents blocking operations

### CPU Efficiency
- Event-driven architecture minimizes CPU usage when idle
- Efficient JSON parsing for message routing
- Lazy evaluation of stream subscriptions

## Limitations and Considerations

### Binance-Specific Design
- Optimized for Binance WebSocket API format and requirements
- May require modifications for other cryptocurrency exchanges

### Single Connection Model
- All streams share one WebSocket connection
- Connection failure affects all subscribed streams
- May hit Binance rate limits with too many streams

### Synchronous Message Sending
- `send_message()` is synchronous and may block briefly
- Consider async alternatives for high-frequency message sending

## Dependencies

### Required Libraries
- **Boost.Beast**: WebSocket and HTTP client functionality
- **Boost.Asio**: Asynchronous I/O and networking
- **OpenSSL**: SSL/TLS encryption support

### Standard Library
- **std::chrono**: Time and duration handling
- **std::mutex**: Thread synchronization
- **std::regex**: Message parsing (optional, depending on implementation)
- **std::functional**: Callback function support

## Best Practices

### Resource Management
```cpp
// Always use shared_ptr for Connection instances
auto connection = std::make_shared<Connection>(ioc);

// Ensure IO context runs in a dedicated thread
std::thread io_thread([&ioc]() { ioc.run(); });

// Proper shutdown
connection->stop();
ioc.stop();
if (io_thread.joinable()) {
    io_thread.join();
}
```

### Error Handling
```cpp
// Always set error callbacks
connection->set_error_callback([](const std::string& error) {
    // Log error appropriately
    spdlog::error("Connection error: {}", error);
});
```

### Performance Optimization
```cpp
// Reserve space for known number of streams
// (This is handled internally by the class)

// Use efficient callback implementations
connection->subscribe_stream("btcusdt@depth", 
    [](const std::string& stream_name, const std::string& data) {
        // Avoid expensive operations in callbacks
        // Consider using a message queue for heavy processing
    });
```

## Troubleshooting

### Common Issues

1. **Connection Timeouts**
   - Check network connectivity
   - Verify Binance server availability
   - Increase timeout values in configuration

2. **SSL/TLS Errors**
   - Ensure system has up-to-date SSL certificates
   - Check system time is synchronized
   - Verify OpenSSL installation

3. **Subscription Failures**
   - Verify stream name format (symbol@streamType)
   - Check Binance API documentation for valid stream types
   - Ensure connection is established before subscribing

4. **Memory Leaks**
   - Use proper RAII patterns with smart pointers
   - Ensure all callbacks use weak_ptr to avoid circular references
   - Call stop() before destroying Connection instances

### Debugging

Enable debug output by checking the console messages:
- Connection attempts and results
- Subscription/unsubscription confirmations
- Error messages with detailed descriptions
- Ping/pong activity logs
