#include <string>
#include <memory>
#include <iostream>
#include <boost/asio.hpp>

#include "ticker.hpp"
#include "connection.hpp"

const std::vector<std::string> ticker_symbols = {"usdcusdt", "btcusdt",  "ethusdt"};

// List of ticker symbols
const std::vector<std::string> symbols_matrix = {
    // "usdcusdt", "btcusdt",  "ethusdt",  "bnbusdt",
    "-",        "btcusdc",  "ethusdc",  "bnbusdc",
    "-",        "-",        "ethbtc",   "bnbbtc", 
    // "-",        "-",        "-",        "bnbeth", 
    "-",        "-",        "-",        "-",      
};

// List of ticker symbols
const std::vector<std::string> symbols_matrix2 = {
    "usdcusdt", "btcusdt",  "ethusdt",  "bnbusdt",  "adausdt",  "solusdt",  "xrpusdt",
    "-",        "btcusdc",  "ethusdc",  "bnbusdc",  "adausdc",  "solusdc",  "xrpusdc",
    "-",        "-",        "ethbtc",   "bnbbtc",   "adabtc",   "solbtc",   "xrpbtc"
    "-",        "-",        "-",        "bnbeth",   "adaeth",   "soleth",   "xrpeth",
    "-",        "-",        "-",        "-",        "adabnb",   "solbnb",   "xrpbnb",
};

const std::vector<std::vector<float>> prices_matrix = {
    {  0.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0 },
    {  1.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0 },
    {  1.0,  1.0,  0.0,  0.0,  0.0,  0.0,  0.0 },
    {  1.0,  1.0,  1.0,  1.0,  0.0,  0.0,  0.0 },
    {  1.0,  1.0,  1.0,  1.0,  0.0,  0.0,  0.0 },
};

int main() {
    try {
        // I/O context
        boost::asio::io_context ioc;

        // Create connection configuration
        grab::connection::Config config;
        config.host = "stream.binance.com";
        config.port = "443";
        config.endpoint = "/ws";
        config.use_ssl = true;

        // Create connection instance
        auto connection = std::make_shared<grab::connection::Connection>(ioc, config);

        // Store ticker instances to keep them alive and subscribe after connection
        std::vector<std::shared_ptr<grab::ticker::Ticker>> tickers;

        // Create all ticker instances but don't subscribe yet
        for (const auto& ticker_symbol : symbols_matrix) {
            if (ticker_symbol == "-") { 
                continue;
            }

            // Create ticker wrapper
            auto ticker = std::make_shared<grab::ticker::Ticker>(connection, ticker_symbol, "bookTicker");
            
            // Set up message callback
            ticker->set_message_callback([ticker_symbol](const std::string& message) {
                std::cout << "[" << ticker_symbol << "] Received: " << message.substr(0, 100) << "..." << std::endl;
            });

            // Set up error callback
            ticker->set_error_callback([ticker_symbol](const std::string& error) {
                std::cerr << "[" << ticker_symbol << "] Error: " << error << std::endl;
            });
            
            // Store the ticker instance for later subscription
            tickers.push_back(ticker);
            
            std::cout << "Created ticker for " << ticker_symbol << " (waiting for connection)" << std::endl;
        }

        // Set up connection event callbacks
        connection->set_connection_callbacks(
            [&tickers]() {
                std::cout << "Connected to Binance WebSocket!" << std::endl;
                std::cout << "Now subscribing to ticker streams..." << std::endl;
                
                // Subscribe to all tickers now that connection is established
                for (auto& ticker : tickers) {
                    ticker->subscribe();

                    // wait
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }

                std::cout << "All ticker subscriptions sent!" << std::endl;
            },
            []() {
                std::cout << "Disconnected from Binance WebSocket" << std::endl;
            }
        );

        // Set up error callback
        connection->set_error_callback([](const std::string& error) {
            std::cerr << "Connection Error: " << error << std::endl;
        });

        // Start the connection (tickers will be subscribed in the connection callback)
        std::cout << "Starting WebSocket connection..." << std::endl;
        connection->start();

        // Run the I/O context to process async operations
        std::cout << "Press Ctrl+C to stop." << std::endl;
        
        // Run the I/O context to process async operations
        ioc.run();
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}