# Ticker Class Usage

The `Ticker` class is a wrapper that encapsulates a WebSocket connection for streaming data from a single ticker symbol.

## Features

- **WebSocket Management**: Encapsulates WebSocket connection lifecycle
- **Async Operations**: Non-blocking message reading with callback support
- **Error Handling**: Comprehensive error handling with custom error callbacks
- **Thread Safety**: Uses shared_ptr for safe async operations
- **RAII Design**: Automatic cleanup on destruction

## Basic Usage

```cpp
#include "ticker.hpp"

// 1. Create and establish WebSocket connection
auto ws = std::make_shared<websocket::stream<ssl::stream<tcp::socket>>>(ioc, ctx);
// ... establish connection (TCP, SSL, WebSocket handshake)

// 2. Create ticker wrapper
auto ticker = std::make_shared<Ticker>(ws, "btcusdt");

// 3. Set up callbacks
ticker->set_message_callback([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
});

ticker->set_error_callback([](const std::string& error) {
    std::cerr << "Error: " << error << std::endl;
});

// 4. Subscribe to data stream
std::string subscription = R"({"method": "SUBSCRIBE", "params": ["btcusdt@depth"], "id": 1})";
ticker->subscribe(subscription);

// 5. Start listening
ticker->start_listening();

// 6. Run I/O context
ioc.run();
```

## Class Interface

### Constructor
```cpp
Ticker(std::shared_ptr<WebSocketStream> ws, const std::string& ticker_symbol);
```

### Key Methods
- `subscribe(const std::string& subscription_message)` - Send subscription message
- `start_listening()` - Begin async message reading
- `stop()` - Stop listening and close connection
- `set_message_callback(MessageCallback callback)` - Set message handler
- `set_error_callback(ErrorCallback callback)` - Set error handler
- `is_connected()` - Check connection status
- `get_ticker_symbol()` - Get the ticker symbol

### Callback Types
```cpp
using MessageCallback = std::function<void(const std::string& message)>;
using ErrorCallback = std::function<void(const std::string& error)>;
```

## Design Notes

- The class uses `std::enable_shared_from_this` for safe async operations
- WebSocket connection is passed as shared_ptr to allow sharing between multiple tickers
- Non-copyable but moveable for efficient resource management
- Automatic cleanup in destructor ensures proper connection closure
