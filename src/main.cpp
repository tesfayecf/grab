#include <regex>
#include <string>
#include <memory>
#include <iostream>
#include <boost/asio.hpp>

#include "ticker.hpp"
#include "connection.hpp"

const std::vector<std::string> ticker_symbols = {"usdcusdt", "btcusdt", "ethusdt"};

// List of ticker symbols
// clang-format off
const std::vector<std::string> symbols_matrix = {
    "-",    "usdcusdt", "btcusdt",  "ethusdt",  "bnbusdt",
    "-",    "-",        "btcusdc",  "ethusdc",  "bnbusdc",
    "-",    "-",        "-",        "ethbtc",   "bnbbtc",
    "-",    "-",        "-",        "-",        "bnbeth",
    "-",    "-",        "-",        "-",        "-", 
};
// clang-format on

// Maxtrix of prices
// clang-format off
std::vector<float> prices_matrix = {
    1.0,    0.0,    0.0,    0.0,    0.0,
    0.0,    1.0,    0.0,    0.0,    0.0,
    0.0,    0.0,    1.0,    0.0,    0.0,
    0.0,    0.0,    0.0,    1.0,    0.0,
    0.0,    0.0,    0.0,    0.0,    1.0,
};
// clang-format on

// List of ticker symbols
// clang-format off
const std::vector<std::string> symbols_matrix2 = {
    "-",    "usdcusdt", "btcusdt",  "ethusdt",  "bnbusdt",  "adausdt",  "solusdt",  "xrpusdt",
    "-",    "-",        "btcusdc",  "ethusdc",  "bnbusdc",  "adausdc",  "solusdc",  "xrpusdc",
    "-",    "-",        "-",        "ethbtc",   "bnbbtc",   "adabtc",   "solbtc",   "xrpbtc"
    "-",    "-",        "-",        "-",        "bnbeth",   "adaeth",   "soleth",   "xrpeth",
    "-",    "-",        "-",        "-",        "-",        "adabnb",   "solbnb",   "xrpbnb",
    "-",    "-",        "-",        "-",        "-",        "-",        "-",        "-",
    "-",    "-",        "-",        "-",        "-",        "-",        "-",        "-",
    "-",    "-",        "-",        "-",        "-",        "-",        "-",        "-",
};
// clang-format off

// Maxtrix of prices
// clang-format off
std::vector<float> prices_matrix2 = {
    1.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,
    0.0,    1.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,
    0.0,    0.0,    1.0,    0.0,    0.0,    0.0,    0.0,    0.0,
    0.0,    0.0,    0.0,    1.0,    0.0,    0.0,    0.0,    0.0,
    0.0,    0.0,    0.0,    0.0,    1.0,    0.0,    0.0,    0.0,
    0.0,    0.0,    0.0,    0.0,    0.0,    1.0,    0.0,    0.0,
    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    1.0,    0.0,
    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    1.0,
};
// clang-format on

const int ROWS = 8;
const int COLS = 8;

void update_matrix(std::string symbol, float price, bool is_bid = true)
{
    // Find index of symbol in matrix
    int index = std::find(symbols_matrix.begin(), symbols_matrix.end(), symbol) - symbols_matrix.begin();
    if (is_bid)
    {
        // Update price in matrix
        prices_matrix[index] = price;
    }
    else
    {
        // Compute ask price
        float ask_price = 1.0 / price;
        // Get symetric index
        int row = index / COLS;
        int col = index % COLS;
        int s_row = COLS - 1 - row;
        int s_col = COLS - 1 - col;
        int s_index = s_row * COLS + s_col;

        // Update ask price in matrix
        prices_matrix[s_index] = ask_price;
    }
}
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

void print_matrix()
{
    std::system("clear");

    const int width = 10;       // cell width
    const int precision = 2;    // decimals

    auto print_separator = [&](int cols) {
        std::cout << "+";
        for (int j = 0; j < cols + 1; j++)
            std::cout << std::string(width, '-') << "+";
        std::cout << "\n";
    };

    // Print top border + headers
    print_separator(COLS);

    std::cout << "|" << std::setw(width) << " ";
    for (int j = 0; j < COLS; j++)
        std::cout << "|" << std::setw(width) << ("C" + std::to_string(j));
    std::cout << "|\n";

    // Header separator
    print_separator(COLS);

    // Print each row
    for (int i = 0; i < ROWS; i++)
    {
        std::cout << "|" << std::setw(width) << ("R" + std::to_string(i));
        for (int j = 0; j < COLS; j++)
        {
            std::cout << "|" << std::setw(width)
                      << std::fixed << std::setprecision(precision)
                      << prices_matrix[i * COLS + j];
        }
        std::cout << "|\n";
        print_separator(COLS);
    }
}

int main()
{
    try
    {
        // I/O context
        boost::asio::io_context ioc;
        auto strand = boost::asio::make_strand(ioc);

        // Create connection configuration
        grab::connection::Config config;
        config.host = "stream.binance.com";
        config.port = "443";
        config.endpoint = "/stream";
        config.use_ssl = true;

        // Store ticker instances to keep them alive and subscribe after connection
        std::vector<std::shared_ptr<grab::ticker::Ticker>> tickers;
        std::vector<std::thread> threads;

        // Create all ticker instances but don't subscribe yet
        for (const auto &ticker_symbol : symbols_matrix)
        {
            if (ticker_symbol == "-")
            {
                continue;
            }

            // Create connection instance
            auto connection = std::make_shared<grab::connection::Connection>(ioc, config);

            // Create ticker instance
            auto ticker = std::make_shared<grab::ticker::Ticker>(connection, ticker_symbol, "bookTicker");
            std::cout << "Created ticker instance for " << ticker_symbol << std::endl;

            // Set up connection callbacks
            connection->set_connection_callbacks(
                [ticker_ = ticker]()
                {
                    std::cout << "Connected to " + ticker_->get_ticker_symbol() + " stream!" << std::endl;
                    // Subscribe to the ticker stream
                    ticker_->subscribe();
                },
                [&ticker]()
                {
                    std::cout << "Disconnected from " + ticker->get_ticker_symbol() + " stream!" << std::endl;
                });

            // Set up message callback
            ticker->set_message_callback(
                [ticker_symbol, strand](const std::string &message)
                {
                    try
                    {

                        // Get "data" from message with regex
                        std::regex data_regex("\"data\":(.*?)}");
                        std::smatch data_match;
                        std::regex_search(message, data_match, data_regex);
                        std::string data_str = data_match.str(1);

                        // Get "b" from message with regex
                        std::regex b_regex("\"b\":(.*?),");
                        std::smatch b_match;
                        std::regex_search(data_str, b_match, b_regex);
                        std::string b_str = b_match.str(1);
                        // Remove `\"`
                        b_str = b_str.substr(2, b_str.length() - 4);
                        // Convert value to float
                        float b_value = std::stof(b_str);

                        // Get "a" from message with regex
                        std::regex a_regex("\"a\":(.*?),");
                        std::smatch a_match;
                        std::regex_search(data_str, a_match, a_regex);
                        std::string a_str = a_match.str(1);
                        // Remove `\"`
                        a_str = a_str.substr(2, a_str.length() - 4);
                        // Convert value to float
                        float a_value = std::stof(a_str);

                        boost::asio::post(strand,
                            [ticker_symbol, b_value, a_value]()
                            {
                                // Update matrix
                                update_matrix(ticker_symbol, b_value);
                                update_matrix(ticker_symbol, a_value, false);
                                
                                // Print matrix
                                print_matrix();
                            }
                        );
                    }
                    catch (const std::exception &e)
                    {
                        return;
                    }
                }
            );

            // Set up error callback
            connection->set_error_callback(
                [](const std::string &error)
                {
                    std::cerr << "Connection Error: " << error << std::endl;
                }
            );

            // Set up error callback
            ticker->set_error_callback(
                [ticker_symbol](const std::string &error)
                {
                    std::cerr << "[" << ticker_symbol << "] Error: " << error << std::endl;
                }
            );

            // Start the connection
            connection->start();

            // Create thread for ticker
            std::thread thread([&ioc]() {
                // Run the I/O context to process async operations
                ioc.run();
            });

            // Store the thread for later joining
            threads.push_back(std::move(thread));

            // Store the ticker instance for later subscription
            tickers.push_back(ticker);

            // Wait a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Run the I/O context to process async operations
        // ioc.run();

        // Wait for all threads to finish
        for (auto &thread : threads)
        {
            thread.join();
        }
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}
