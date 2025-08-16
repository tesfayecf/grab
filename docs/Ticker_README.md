# Ticker Class

## Overview

The `Ticker` class is a high-level wrapper that provides a simple interface for subscribing to individual cryptocurrency data streams. It encapsulates the complexity of WebSocket connection management by using a shared `Connection` instance, making it easy to monitor specific trading pairs and data types.

## Key Features

- **Simplified Interface**: Easy-to-use wrapper for individual ticker monitoring
- **Shared Connection**: Multiple tickers can efficiently share a single WebSocket connection
- **Stream-Specific Callbacks**: Dedicated message and error handling for each ticker
- **Automatic Lifecycle Management**: Handles subscription and cleanup automatically
- **Type Safety**: Strong typing for ticker symbols and stream types
- **Resource Efficiency**: Minimal overhead per ticker instance

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
├─────────────────┬─────────────────┬─────────────────────────┤
│   Ticker        │   Ticker        │   Ticker                │
│   (BTCUSDT)     │   (ETHUSDT)     │   (ADAUSDT)             │
│   @depth        │   @trade        │   @kline_1m             │
└─────────┬───────┴─────────┬───────┴─────────────┬───────────┘
          │                 │                     │
          └─────────────────┼─────────────────────┘
                            │
                   ┌────────▼────────┐
                   │   Connection    │
                   │   (Shared)      │
                   └────────┬────────┘
                            │
                   ┌────────▼────────┐
                   │  Binance API    │
                   │  WebSocket      │
                   └─────────────────┘
```

## Core Concepts

### Stream Naming Convention
Binance uses a specific format for stream names: `symbol@streamType`
- **Symbol**: Lowercase trading pair (e.g., `btcusdt`, `ethbtc`)
- **Stream Type**: Data type (e.g., `depth`, `trade`, `kline_1m`, `bookTicker`)
- **Examples**: `btcusdt@depth`, `ethusdt@trade`, `adausdt@kline_1m`

### Supported Stream Types
- **depth**: Order book depth updates
- **trade**: Individual trade executions
- **kline_1m/5m/1h/1d**: Candlestick/kline data at various intervals
- **bookTicker**: Best bid/ask price updates
- **ticker**: 24-hour ticker statistics
- **miniTicker**: Mini 24-hour ticker statistics

### Connection Sharing
Multiple `Ticker` instances can share the same `Connection` object:
- Reduces network overhead
- Simplifies connection management
- Allows coordinated error handling
- Enables efficient resource utilization

## Usage Examples

### Basic Ticker Setup

```cpp
#include "ticker.hpp"
#include "connection.hpp"
#include <boost/asio.hpp>

// Create IO context and shared connection
boost::asio::io_context ioc;
auto connection = std::make_shared<Connection>(ioc);

// Create ticker for Bitcoin depth data
auto btc_ticker = std::make_shared<Ticker>(connection, "btcusdt", "depth");

// Set message callback
btc_ticker->set_message_callback([](const std::string& message) {
    std::cout << "BTC Depth Update: " << message << std::endl;
});

// Set error callback
btc_ticker->set_error_callback([](const std::string& error) {
    std::cerr << "BTC Ticker Error: " << error << std::endl;
});

// Start the ticker
btc_ticker->start();

// Start the connection
connection->start();

// Run the IO context
ioc.run();
```

### Multiple Tickers with Shared Connection

```cpp
// Create shared connection
auto connection = std::make_shared<Connection>(ioc);

// Create multiple tickers for different pairs and data types
auto btc_depth = std::make_shared<Ticker>(connection, "btcusdt", "depth");
auto eth_trades = std::make_shared<Ticker>(connection, "ethusdt", "trade");
auto ada_klines = std::make_shared<Ticker>(connection, "adausdt", "kline_1m");

// Set up callbacks for each ticker
btc_depth->set_message_callback([](const std::string& message) {
    // Handle BTC order book updates
    std::cout << "BTC Depth: " << message << std::endl;
});

eth_trades->set_message_callback([](const std::string& message) {
    // Handle ETH trade executions
    std::cout << "ETH Trade: " << message << std::endl;
});

ada_klines->set_message_callback([](const std::string& message) {
    // Handle ADA 1-minute kline data
    std::cout << "ADA Kline: " << message << std::endl;
});

// Start all tickers
btc_depth->start();
eth_trades->start();
ada_klines->start();

// Start the shared connection
connection->start();
```

### Dynamic Ticker Management

```cpp
class TradingBot {
private:
    boost::asio::io_context ioc_;
    std::shared_ptr<Connection> connection_;
    std::vector<std::shared_ptr<Ticker>> tickers_;

public:
    TradingBot() : connection_(std::make_shared<Connection>(ioc_)) {
        connection_->start();
    }

    void add_ticker(const std::string& symbol, const std::string& stream_type) {
        auto ticker = std::make_shared<Ticker>(connection_, symbol, stream_type);
        
        ticker->set_message_callback([this, symbol](const std::string& message) {
            handle_market_data(symbol, message);
        });
        
        ticker->set_error_callback([this, symbol](const std::string& error) {
            handle_ticker_error(symbol, error);
        });
        
        ticker->start();
        tickers_.push_back(ticker);
    }

    void remove_ticker(const std::string& symbol, const std::string& stream_type) {
        std::string stream_name = symbol + "@" + stream_type;
        
        auto it = std::find_if(tickers_.begin(), tickers_.end(),
            [&stream_name](const auto& ticker) {
                return ticker->get_stream_name() == stream_name;
            });
        
        if (it != tickers_.end()) {
            (*it)->stop();
            tickers_.erase(it);
        }
    }

private:
    void handle_market_data(const std::string& symbol, const std::string& data) {
        // Process market data for trading decisions
    }
    
    void handle_ticker_error(const std::string& symbol, const std::string& error) {
        // Handle ticker-specific errors
    }
};
```

### Advanced Error Handling

```cpp
class RobustTicker {
private:
    std::shared_ptr<Ticker> ticker_;
    std::string symbol_;
    std::string stream_type_;
    int reconnect_attempts_;
    static const int MAX_RECONNECT_ATTEMPTS = 5;

public:
    RobustTicker(std::shared_ptr<Connection> connection, 
                 const std::string& symbol, 
                 const std::string& stream_type)
        : symbol_(symbol), stream_type_(stream_type), reconnect_attempts_(0) {
        
        create_ticker(connection);
    }

private:
    void create_ticker(std::shared_ptr<Connection> connection) {
        ticker_ = std::make_shared<Ticker>(connection, symbol_, stream_type_);
        
        ticker_->set_message_callback([this](const std::string& message) {
            // Reset reconnect attempts on successful message
            reconnect_attempts_ = 0;
            handle_message(message);
        });
        
        ticker_->set_error_callback([this, connection](const std::string& error) {
            handle_error(error, connection);
        });
        
        ticker_->start();
    }
    
    void handle_message(const std::string& message) {
        // Process the ticker message
        std::cout << "[" << symbol_ << "] " << message << std::endl;
    }
    
    void handle_error(const std::string& error, std::shared_ptr<Connection> connection) {
        std::cerr << "[" << symbol_ << "] Error: " << error << std::endl;
        
        if (reconnect_attempts_ < MAX_RECONNECT_ATTEMPTS) {
            reconnect_attempts_++;
            std::cout << "[" << symbol_ << "] Reconnection attempt " 
                      << reconnect_attempts_ << "/" << MAX_RECONNECT_ATTEMPTS << std::endl;
            
            // Recreate ticker after a delay
            std::this_thread::sleep_for(std::chrono::seconds(5));
            create_ticker(connection);
        } else {
            std::cerr << "[" << symbol_ << "] Max reconnection attempts reached" << std::endl;
        }
    }
};
```

## Public Interface

### Constructor
```cpp
Ticker(std::shared_ptr<Connection> connection, 
       const std::string& ticker_symbol, 
       const std::string& stream_type = "depth");
```
- **connection**: Shared connection instance
- **ticker_symbol**: Trading pair symbol (lowercase, e.g., "btcusdt")
- **stream_type**: Data stream type (default: "depth")

### Callback Management
```cpp
void set_message_callback(MessageCallback callback);
void set_error_callback(ErrorCallback callback);
```
- Set callbacks for message and error handling
- Callbacks are invoked on the IO context thread

### Lifecycle Control
```cpp
void start();    // Begin subscription
void stop();     // End subscription
```
- `start()`: Registers with connection and begins receiving data
- `stop()`: Unregisters from connection and stops data flow

### Information Access
```cpp
const std::string& get_ticker_symbol() const;
const std::string& get_stream_name() const;
bool is_subscribed() const;
```
- Access ticker configuration and status information

## Memory Management

### Smart Pointer Usage
The Ticker class uses smart pointers to prevent memory leaks and circular references:

```cpp
// Ticker holds shared_ptr to Connection
std::shared_ptr<Connection> connection_;

// Callbacks use weak_ptr to avoid circular references
[weak_self = std::weak_ptr<Ticker>(shared_from_this())](/* parameters */) {
    if (auto self = weak_self.lock()) {
        // Safe to use self here
    }
    // If lock() returns null, ticker has been destroyed
}
```

### RAII Pattern
- Constructor validates inputs and initializes state
- Destructor automatically calls `stop()` for cleanup
- Move semantics supported for efficient transfers

## Thread Safety

### Connection Sharing
- Multiple tickers can safely share a connection
- Connection class handles thread synchronization
- Ticker operations are atomic with respect to the connection

### Callback Execution
- All callbacks execute on the IO context thread
- No additional synchronization needed in callbacks
- Avoid blocking operations in callback functions

## Error Handling

### Error Categories

1. **Construction Errors**
   - Null connection pointer
   - Empty ticker symbol
   - Empty stream type

2. **Subscription Errors**
   - Connection not available
   - Invalid stream name
   - Server rejection

3. **Runtime Errors**
   - Connection loss
   - Message parsing failures
   - Protocol violations

### Error Recovery Strategies

```cpp
// Retry with exponential backoff
void exponential_backoff_retry(int attempt) {
    int delay = std::pow(2, attempt);  // 2^attempt seconds
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    ticker->start();
}

// Circuit breaker pattern
class CircuitBreaker {
    int failure_count_ = 0;
    bool circuit_open_ = false;
    
public:
    bool should_attempt() {
        if (circuit_open_ && failure_count_ > 5) {
            return false;  // Circuit is open
        }
        return true;
    }
    
    void on_success() {
        failure_count_ = 0;
        circuit_open_ = false;
    }
    
    void on_failure() {
        failure_count_++;
        if (failure_count_ > 5) {
            circuit_open_ = true;
        }
    }
};
```

## Performance Considerations

### Message Processing
- Keep callback functions lightweight
- Use message queues for heavy processing
- Avoid blocking operations in callbacks

```cpp
// Good: Lightweight callback
ticker->set_message_callback([](const std::string& message) {
    message_queue.push(message);  // Quick operation
});

// Bad: Heavy processing in callback
ticker->set_message_callback([](const std::string& message) {
    complex_analysis(message);    // Blocks the IO thread
    database_write(message);      // Slow operation
});
```

### Resource Usage
- Each ticker has minimal memory overhead
- Shared connection reduces network resources
- Efficient stream routing minimizes CPU usage

## Integration Patterns

### Observer Pattern
```cpp
class MarketDataObserver {
public:
    virtual void on_depth_update(const std::string& symbol, const std::string& data) = 0;
    virtual void on_trade_update(const std::string& symbol, const std::string& data) = 0;
};

class TickerManager {
    std::vector<MarketDataObserver*> observers_;
    
public:
    void add_observer(MarketDataObserver* observer) {
        observers_.push_back(observer);
    }
    
    void setup_ticker(const std::string& symbol, const std::string& stream_type) {
        auto ticker = std::make_shared<Ticker>(connection_, symbol, stream_type);
        
        ticker->set_message_callback([this, symbol, stream_type](const std::string& data) {
            for (auto observer : observers_) {
                if (stream_type == "depth") {
                    observer->on_depth_update(symbol, data);
                } else if (stream_type == "trade") {
                    observer->on_trade_update(symbol, data);
                }
            }
        });
        
        ticker->start();
    }
};
```

### Event-Driven Architecture
```cpp
struct MarketEvent {
    std::string symbol;
    std::string stream_type;
    std::string data;
    std::chrono::system_clock::time_point timestamp;
};

class EventBus {
    boost::asio::io_context& ioc_;
    std::function<void(const MarketEvent&)> handler_;
    
public:
    void subscribe(std::function<void(const MarketEvent&)> handler) {
        handler_ = handler;
    }
    
    void publish(const MarketEvent& event) {
        if (handler_) {
            boost::asio::post(ioc_, [this, event]() {
                handler_(event);
            });
        }
    }
};
```

## Testing

### Unit Testing
```cpp
class MockConnection : public Connection {
public:
    MOCK_METHOD(void, subscribe_stream, 
                (const std::string&, MessageCallback), (override));
    MOCK_METHOD(void, unsubscribe_stream, 
                (const std::string&), (override));
    MOCK_METHOD(bool, is_connected, (), (const, override));
};

TEST(TickerTest, SubscriptionLifecycle) {
    auto mock_connection = std::make_shared<MockConnection>();
    
    EXPECT_CALL(*mock_connection, subscribe_stream("btcusdt@depth", _))
        .Times(1);
    EXPECT_CALL(*mock_connection, unsubscribe_stream("btcusdt@depth"))
        .Times(1);
    
    auto ticker = std::make_shared<Ticker>(mock_connection, "btcusdt", "depth");
    ticker->start();
    ticker->stop();
}
```

### Integration Testing
```cpp
class IntegrationTest {
    boost::asio::io_context ioc_;
    std::shared_ptr<Connection> connection_;
    
public:
    void test_real_data_flow() {
        connection_ = std::make_shared<Connection>(ioc_);
        auto ticker = std::make_shared<Ticker>(connection_, "btcusdt", "depth");
        
        bool message_received = false;
        ticker->set_message_callback([&message_received](const std::string& data) {
            message_received = true;
        });
        
        ticker->start();
        connection_->start();
        
        // Run for a short time to receive messages
        auto timer = std::make_shared<boost::asio::steady_timer>(ioc_);
        timer->expires_after(std::chrono::seconds(10));
        timer->async_wait([&](boost::system::error_code) {
            ioc_.stop();
        });
        
        ioc_.run();
        ASSERT_TRUE(message_received);
    }
};
```

## Best Practices

### Lifecycle Management
```cpp
// Always use shared_ptr for tickers
auto ticker = std::make_shared<Ticker>(connection, "btcusdt", "depth");

// Ensure proper cleanup
ticker->stop();  // Explicit stop if needed
// Destructor will call stop() automatically
```

### Error Handling
```cpp
// Always set error callbacks
ticker->set_error_callback([](const std::string& error) {
    spdlog::error("Ticker error: {}", error);
});
```

### Performance
```cpp
// Use efficient message processing
ticker->set_message_callback([this](const std::string& message) {
    // Parse and queue for processing
    auto parsed = json::parse(message);
    data_processor_.enqueue(std::move(parsed));
});
```

## Common Pitfalls

1. **Forgetting to start the connection**: Tickers need an active connection
2. **Blocking in callbacks**: Keep callbacks fast and non-blocking
3. **Memory leaks**: Use shared_ptr and avoid circular references
4. **Invalid stream names**: Follow Binance naming conventions exactly
5. **Missing error handling**: Always implement error callbacks

## Troubleshooting

### No Data Received
- Verify connection is started and connected
- Check stream name format (symbol@streamType)
- Ensure ticker is started after setting callbacks
- Verify Binance stream type is valid

### Memory Issues
- Check for circular references between ticker and callbacks
- Ensure proper cleanup in destructors
- Use weak_ptr in async operations

### Performance Problems
- Move heavy processing out of callbacks
- Use message queues for decoupling
- Monitor connection for multiple concurrent streams
