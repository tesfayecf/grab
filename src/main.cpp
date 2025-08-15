#include <string>
#include <memory>
#include <iostream>
#include <boost/asio.hpp>

#include "ticker.hpp"
#include "connection.hpp"

// List of ticker symbols
const std::vector<std::string> tickers = {"btcusdt", "ethusdt", "ltcusdt", "bchusdt"};

int main() {
    try {
        std::string ticker_symbol = "btcusdt";

        // I/O context
        boost::asio::io_context ioc;

        // Create connection configuration
        Connection::Config config;
        config.host = "stream.binance.com";
        config.port = "9443";
        config.use_ssl = true;

        // Create connection instance
        auto connection = std::make_shared<Connection>(ioc, config);

        // Set up connection event callbacks
        connection->set_connection_callbacks(
            []() {
                std::cout << "Connected to Binance WebSocket!" << std::endl;
            },
            []() {
                std::cout << "Disconnected from Binance WebSocket" << std::endl;
            }
        );

        // Set up error callback
        connection->set_error_callback([](const std::string& error) {
            std::cerr << "Connection Error: " << error << std::endl;
        });

        // Create and start tickers
        for (auto& ticker_symbol : tickers) {

            // Create ticker wrapper
            auto ticker = std::make_shared<Ticker>(connection, ticker_symbol, "bookTicker");
            
            // Set up message callback
            ticker->set_message_callback([&ticker_symbol](const std::string& message) {
                std::cout << "[" << ticker_symbol << "] Received: " << message.substr(0, 100) << "..." << std::endl;
            });

            // Set up error callback
            ticker->set_error_callback([&ticker_symbol](const std::string& error) {
                std::cerr << "[" << ticker_symbol << "] Error: " << error << std::endl;
            });
            
            // Start ticker (this will register it with the connection)
            ticker->start();

            // Output message
            std::cout << "Starting ticker stream for " << ticker_symbol << "..." << std::endl;
        }

        // Start the connection
        connection->start();

        // Run the I/O context to process async operations
        std::cout << "Press Ctrl+C to stop." << std::endl;
        
        ioc.run();

    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}