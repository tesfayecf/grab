#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <regex>
#include <memory>
#include <chrono>
#include <thread>
#include <algorithm>

#include <boost/asio.hpp>
#include <json-nlohmann.hpp>

#include "ticker.hpp"
#include "connection.hpp"
#include "./graph/graph.hpp"
#include "./graph/graph-bf.hpp"

// List of ticker symbols
// clang-format off
const int NUM_SYMBOLS = 14;
const std::vector<std::string> symbols = {
    "usdt", "usdc",     "btc",      "eth",      "bnb",      "ada",      "sol",      "xrp",      "trx",      "link",     "bch",      "sui",      "avax",     "hbar",
};
const std::vector<std::string> tickers_matrix = {
    "-",    "usdcusdt", "btcusdt",  "ethusdt",  "bnbusdt",  "adausdt",  "solusdt",  "xrpusdt",  "trxusdt",  "linkusdt", "bchusdt",  "suiusdt",  "avaxusdt", "hbarusdt",
    "-",    "-",        "btcusdc",  "ethusdc",  "bnbusdc",  "adausdc",  "solusdc",  "xrpusdc",  "trxusdc",  "linkusdc", "bchusdc",  "suiusdc",  "avaxusdc", "hbarusdc",
    "-",    "-",        "-",        "ethbtc",   "bnbbtc",   "adabtc",   "solbtc",   "xrpbtc",   "trxbtc",   "linkbtc",  "bchbtc",   "suibtc",   "avaxbtc",  "hbarbtc",
    "-",    "-",        "-",        "-",        "bnbeth",   "adaeth",   "soleth",   "xrpeth",   "trxeth",   "linketh",  "-",        "-",        "avaxeth",  "hbareth",
    "-",    "-",        "-",        "-",        "-",        "adabnb",   "solbnb",   "xrpbnb",   "trxbnb",   "linkbnb",  "bchbnb",   "suibnb",   "avaxbnb",  "hbarbnb",
    "-",    "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",
    "-",    "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",
    "-",    "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",        "-",
};
// clang-format off

// Maxtrix of prices
// clang-format off
std::vector<double> prices_matrix = {
    1.0,    0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,        0.0,
    0.0,    0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        0.0,        1.0,
};
// clang-format on

// // List of ticker symbols
// // clang-format off
// const std::vector<std::string> symbols = {
//     "btc",      "bnb",      "trx",      "bch",      "hbar",
// };
// const std::vector<std::string> tickers_matrix = {
//     "-",        "bnbbtc",   "trxbtc",   "bchbtc",   "hbarbtc",
//     "-",        "-",        "trxbnb",   "bchbnb",   "hbarbnb",
//     "-",        "-",        "-",        "-",        "-",
//     "-",        "-",        "-",        "-",        "-",
//     "-",        "-",        "-",        "-",        "-",
// };
// // clang-format off

// // Maxtrix of prices
// // clang-format off
// std::vector<double> prices_matrix = {
//     1.0,        0.0,        0.0,        0.0,        0.0,
//     0.0,        1.0,        0.0,        0.0,        0.0,
//     0.0,        0.0,        1.0,        0.0,        0.0,
//     0.0,        0.0,        0.0,        1.0,        0.0,
//     0.0,        0.0,        0.0,        0.0,        1.0,
// };
// // clang-format on

// const int ROWS = 5;
// const int COLS = 5;

// Global graph instance
std::unique_ptr<grab::graph::Graph> g_price_graph;

int update_matrix(std::string ticker, double bid, double ask)
{
    int index = std::find(tickers_matrix.begin(), tickers_matrix.end(), ticker) - tickers_matrix.begin();
    // Find index of symbol in matrix
    int row = index / NUM_SYMBOLS;
    int col = index % NUM_SYMBOLS;
    if (row == col)
    {
        std::cerr << "Index of ticker: " << index << std::endl;
        std::cerr << "Ticker: " << ticker << std::endl;
        std::cerr << "Row: " << row << " Col: " << col << std::endl;
        std::cout << "Bid: " << bid << " Ask: " << ask << std::endl;
    }
    // Update price in matrix
    prices_matrix[index] = ask;
    // Compute ask price
    double ask_price = 1.0 / bid;
    // Get diagonal symetric index
    prices_matrix[col * NUM_SYMBOLS + row] = ask_price;
    
    // Update the graph with new prices
    if (g_price_graph) {
        // Update direct edge (row -> col) with ask price
        if (ask > 0.0) {
            double log_weight = -std::log(ask);
            g_price_graph->update_edge_weight(row, col, log_weight);
        }
        
        // Update reverse edge (col -> row) with inverse bid price
        if (bid > 0.0) {
            double inverse_price = 1.0 / bid;
            double log_weight = -std::log(inverse_price);
            g_price_graph->update_edge_weight(col, row, log_weight);
        }
    }
    
    // Return index
    return index;
}

int opportunities_found = 0;
double max_profit = 0.0f;
double acc_profit = 0.0f;

void check_arbitrage_opportunities()
{
    if (!g_price_graph) return;
    
    static auto last_check = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // Check for arbitrage every 5 seconds to avoid spam
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_check).count() < 5) {
        return;
    }
    last_check = now;
    
    // Find arbitrage opportunities
    auto arbitrage_path = g_price_graph->find_arbitrage_opportunity();
    
    if (!arbitrage_path.empty() && arbitrage_path.size() > 3) {
        // Convert to vertex indices for profit calculation
        std::vector<int> cycle_indices;
        for (size_t i = 0; i < arbitrage_path.size() - 1; ++i) {  // -1 because last element is duplicate
            int idx = g_price_graph->get_vertex_index(arbitrage_path[i]);
            if (idx != -1) {
                cycle_indices.push_back(idx);
            }
        }
        std::vector<double> cycle_prices;
        for (size_t i = 0; i < arbitrage_path.size() - 1; ++i) {
            cycle_prices.push_back(prices_matrix[cycle_indices[i + 1] * NUM_SYMBOLS + cycle_indices[i]]);
        }
        
        if (!cycle_indices.empty()) {
            double profit = g_price_graph->calculate_arbitrage_profit(cycle_indices);
            
            double real_profit = profit - 0.1 * cycle_indices.size();
            if (profit > 0.01) {  // Only show if profit > 0.01%
                // Update stats
                opportunities_found++;
                max_profit = std::max(max_profit, real_profit);
                acc_profit += real_profit;

                std::cout << "\033[1;33m*** ARBITRAGE OPPORTUNITY DETECTED ***\033[0m" << std::endl;
                std::cout << "\033[1;32mPath: ";
                for (size_t i = 0; i < arbitrage_path.size(); ++i) {
                    std::cout << arbitrage_path[i];
                    if (i < arbitrage_path.size() - 1) {
                        std::cout << " -> ";
                    }
                }
                std::cout << "\033[0m" << std::endl;
                std::cout << "\033[1;32mPrices: ";
                for (size_t i = 0; i < cycle_prices.size(); ++i) {
                    std::cout << std::fixed << cycle_prices[i];
                    if (i < cycle_prices.size() - 1) {
                        std::cout << " -> ";
                    }
                }
                std::cout << "\033[0m" << std::endl;

                std::cout << "\033[1;31mCalculated Profit: " << std::fixed << std::setprecision(4) << profit << "%\033[0m" << std::endl;
                std::cout << "\033[1;35mEstimated Profit: " << std::fixed << std::setprecision(4) << real_profit << "%\033[0m" << std::endl;
                std::cout << "Opportunities found: " << opportunities_found << std::endl;
                std::cout << "Max profit: " << std::fixed << std::setprecision(4) << max_profit << "%" << std::endl;
                std::cout << "Accumulated profit: " << std::fixed << std::setprecision(4) << acc_profit << "%" << std::endl;
                std::cout << "\033[1;33m*****************************************\033[0m\n" << std::endl;
            }
        }
    }
}

void check_arbitrage_opportunities_bf()
{
    // Copy matrix
    std::vector<double> prices_matrix_copy = prices_matrix;
    
    // 5 implies the number of nodes in graph
	Graph *g = new Graph(NUM_SYMBOLS);
	// Connect node with an edge
	// First and second parameter indicate node
	// Last parameter is indicate weight

	for (int i = 0; i < NUM_SYMBOLS; i++)
	{
		for (int j = 0; j < NUM_SYMBOLS; j++)
		{
			double &prov = prices_matrix_copy[i * NUM_SYMBOLS + j];
			if (prov > 0)
			{
				g->addEdge(i, j, -log10(prov));
			}
		}
	}

    // Test
	g->minWeightCycle();
}

void print_matrix(int last_index)
{
    std::system("clear");

    const int width = 20;       // cell width
    const int precision = 5;    // decimals

    auto print_separator = [&](int cols) {
        std::cout << "+";
        for (int j = 0; j < cols + 1; j++)
            std::cout << std::string(width, '-') << "+";
        std::cout << "\n";
    };

    // Print top border + headers
    print_separator(NUM_SYMBOLS);

    std::cout << "|" << std::setw(width) << " ";
    for (int j = 0; j < NUM_SYMBOLS; j++)
    {
        std::cout << "|" << std::setw(width) << (symbols[j]);
    }
    std::cout << "|\n";

    // Header separator
    print_separator(NUM_SYMBOLS);

    // Print each row
    for (int i = 0; i < NUM_SYMBOLS; i++)
    {
        std::cout << "|" << std::setw(width) << (symbols[i]);
        for (int j = 0; j < NUM_SYMBOLS; j++)
        {
            if (last_index == i * NUM_SYMBOLS + j)
            {
                // Print with color
                std::cout << "|" << std::setw(width-1) << std::fixed << std::setprecision(precision) << "\033[1;31m" << prices_matrix[i * NUM_SYMBOLS + j] << "\033[0m";
            }
            else if (last_index == j * NUM_SYMBOLS + i)
            {
                // Print with color
                std::cout << "|" << std::setw(width-1) << std::fixed << std::setprecision(precision) << "\033[1;32m" << prices_matrix[i * NUM_SYMBOLS + j] << "\033[0m";
            }
            else
            {
                // Print
                std::cout << "|" << std::setw(width) << std::fixed << std::setprecision(precision) << prices_matrix[i * NUM_SYMBOLS + j];
            }
        }
        std::cout << "|\n";
        print_separator(NUM_SYMBOLS);
    }
}

void print_graph()
{
    std::system("clear");
    g_price_graph->print_graph();
    g_price_graph->print_adjacency_matrix();
}

void test_buy_order()
{
    std::string symbol = "BTCUSDT";
    std::string side = "BUY";
    std::string type = "MARKET";
    std::string timeInForce = "GTC";
    std::string quantity = "0.001";
    std::string price = "50000";
    std::string recvWindow = "10000";
    std::string timestamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count() * 1000); // Current timestamp in milliseconds

    std::string request_body = "";
}


int main()
{
    try
    {
        // Initialize the price graph
        g_price_graph = std::make_unique<grab::graph::Graph>(symbols);
        g_price_graph->build_from_price_matrix(prices_matrix, symbols, NUM_SYMBOLS, NUM_SYMBOLS);
        
        std::cout << "Initialized price graph with " << symbols.size() << " vertices:" << std::endl;
        for (size_t i = 0; i < symbols.size(); ++i) {
            std::cout << "  " << i << ": " << symbols[i] << std::endl;
        }
        std::cout << std::endl;
        
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
        for (const auto &ticker_symbol : tickers_matrix)
        {
            if (ticker_symbol == "-")
            {
                continue;
            }

            // Create connection instance
            auto connection = std::make_shared<grab::connection::Connection>(ioc, config);

            // Create ticker instance
            auto ticker = std::make_shared<grab::ticker::Ticker>(connection, ticker_symbol, "ticker");
            // std::cout << "Created ticker instance for " << ticker_symbol << std::endl;

            // Set up connection callbacks
            connection->set_connection_callbacks(
                [ticker_ = ticker]()
                {
                    // std::cout << "Connected to " + ticker_->get_ticker_symbol() + " stream!" << std::endl;
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
                        // Parse JSON string
                        nlohmann::json j = nlohmann::json::parse(message);

                        // Access data object
                        auto data = j["data"];

                        // Extract values
                        double b_value = std::stod(data["b"].get<std::string>());
                        double a_value = std::stod(data["a"].get<std::string>());

                        boost::asio::post(strand,
                            [ticker_symbol, b_value, a_value]()
                            {
                                // std::cout << "Received message for " << ticker_symbol << ": " << b_value << ", " << a_value << std::endl;
                                // Update matrix
                                int index = update_matrix(ticker_symbol, b_value, a_value);
                                
                                // Check for arbitrage opportunities
                                // check_arbitrage_opportunities_bf();
                                check_arbitrage_opportunities();

                                // Print matrix
                                // print_matrix(index);

                                // Print graph
                                // print_graph();
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
