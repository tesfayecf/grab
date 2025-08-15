#include <iostream>
#include <string>

int main() {
    try {
        std::string host = "stream.binance.com";
        std::string port = "9443";
        std::string target = "/ws/btcusdt@depth";
        std::string message = R"({"method": "SUBSCRIBE", "params": ["btcusdt@depth"], "id": 1})";

        

       
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}