# Grab: Real-time Cryptocurrency Arbitrage Detection Engine

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![CMake Build](https://img.shields.io/badge/CMake-3.31%2B-green.svg?style=flat&logo=cmake)](https://cmake.org)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

`Grab` is a high-performance, real-time cryptocurrency arbitrage detection engine written in modern **C++20**. It establishes concurrent secure WebSocket connections to the Binance live market data stream, builds an dynamic directed asset-exchange graph, and implements graph-theoretic cycle detection algorithms to find profitable triangular and multi-hop arbitrage opportunities.

By transforming live exchange rates into logarithmic weights, the engine models the multiplicative compounding of sequential trades as an additive path-finding problem, making it possible to locate profitable loops instantly using a customized **Bellman-Ford negative-cycle detection** algorithm.

---

## 🚀 Key Features

* **Real-time Live Streaming**: Connects directly to Binance's API via secure WebSockets using `Boost.Asio`, `Boost.Beast`, and `OpenSSL` (TLS 1.3).
* **Asynchronous Multi-Threaded Engine**: Utilizes asynchronous network I/O powered by a thread pool and synchronized via Boost.Asio strands to update exchange rates in a highly thread-safe, lock-free manner.
* **Graph-Theoretic Arbitrage Search**: Represents trading pairs as a dynamic directed graph. Logarithmic edge weights ($-\ln(\text{price})$) enable efficient detection of profitable loops using negative-weight cycle detection.
* **Automatic Session Management**: 
  * Complies with Binance's strict API requirements by maintaining automated heartbeat ping-pong frames.
  * Implements **24-hour connection cycling** to prevent forceful server-side disconnects.
  * Handles unexpected network disruption with automated reconnect logic and exponential backoff.
* **Live Consolidation Matrix**: Dynamic tracking of 14 mainstream cryptocurrency assets (`USDT`, `USDC`, `BTC`, `ETH`, `BNB`, `ADA`, `SOL`, `XRP`, `TRX`, `LINK`, `BCH`, `SUI`, `AVAX`, `HBAR`) in a synchronized cross-rate matrix.
* **Modern C++20 Architecture**: Leverages standard-compliant techniques, RAII smart pointers, precise type-safety, and compiler optimizations (`-O3`, `-march=native`, Link-Time Optimization).

---

## 📐 Theoretical Framework: Logarithmic Arbitrage Search

In standard trading, triangular or cyclic arbitrage is found when starting with an asset $A$, sequentially trading through multiple intermediates, and ending back with more of asset $A$ than we started with. 

Mathematically, for a cycle of exchange rates $P(v_1 \to v_2), P(v_2 \to v_3), \dots, P(v_n \to v_1)$, a profitable trade exists if and only if the product of those rates exceeds 1:
$$P(v_1 \to v_2) \times P(v_2 \to v_3) \times \dots \times P(v_n \to v_1) > 1.0$$

### The Logarithmic Transformation
Multiplying floating-point rates in real-time is computationally inefficient for large-scale path-finding. We can convert this multiplicative problem into an additive one by applying the natural logarithm ($\ln$) to both sides:
$$\ln\left(P(v_1 \to v_2) \times P(v_2 \to v_3) \times \dots \times P(v_n \to v_1)\right) > \ln(1.0)$$

Using log identities, this simplifies to:
$$\ln(P(v_1 \to v_2)) + \ln(P(v_2 \to v_3)) + \dots + \ln(P(v_n \to v_1)) > 0$$

Multiplying the entire inequality by $-1$ reverses the inequality:
$$- \ln(P(v_1 \to v_2)) - \ln(P(v_2 \to v_3)) - \dots - \ln(P(v_n \to v_1)) < 0$$

### Equivalence to Negative-Weight Cycles
By defining the edge weight $w(u, v)$ from asset $u$ to asset $v$ as:
$$w(u, v) = -\ln(P(u \to v))$$

The profit condition translates directly to:
$$\sum_{i=1}^{n} w(v_i, v_{i+1}) < 0$$

This is the exact definition of a **negative-weight cycle**! The engine builds a directed graph where:
1. **Vertices** are assets (e.g. `BTC`, `ETH`, `USDT`).
2. **Edges** represent available trading pairs.
3. **Edge Weights** are set to $-\ln(\text{price})$. For direct trades (buying $B$ with $A$), we use the *ask price*. For inverse trades (selling $B$ for $A$), we use the inverse of the *bid price* ($1 / \text{bid}$).

The **Bellman-Ford algorithm** is then executed over the graph. Since a negative cycle represents an infinite loop of negative weight, any detected cycle corresponds directly to a compounding arbitrage loop.

```
      [ USDT ] 
     /        ^
    /          \ (1/Bid, converted to -ln)
   v            \
[ BTC ] ------> [ ETH ]
         (Ask, converted to -ln)
```

---

## 🗂️ Project Structure

```
grab/
├── cmake/                      # Custom CMake find modules (Boost, OpenSSL, nlohmann_json)
├── docs/                       # Architectural and component design documents
│   ├── Connection.md           # Secure WebSocket Client detail & state machines
│   └── Ticker.md               # Stream-specific Ticker subscription wrapper
├── src/                        # Source Directory
│   ├── main.cpp                # App entrypoint, WebSocket orchestrator, and metrics console
│   ├── connection.hpp/.cpp     # Secure multi-stream TLS WebSocket client using Boost.Beast
│   ├── ticker.hpp/.cpp         # High-level ticker subscription wrapper
│   ├── algorithms/
│   │   └── bellman-ford.cpp    # Reference DFS cyclic & min-weight algorithm helper
│   └── graph/
│       ├── graph.hpp/.cpp      # Core directed graph, adjacency matrix, & Bellman-Ford cycle-finder
│       └── graph-bf.hpp        # Alternative minimum cycle finder with depth-first searches
├── test/                       # Unit and integration test suites
├── CMakeLists.txt              # Top-level CMake configuration
├── CMakePresets.json           # Compilation presets (Release/Debug using Ninja)
└── README.md                   # Project documentation
```

---

## 🛠️ Build and Setup

### Prerequisites

To compile the codebase, ensure you have the following dependencies installed on your system:

* **C++ Compiler**: GCC 10+ or Clang 11+ (C++20 support required)
* **Build System**: CMake 3.31+ and [Ninja](https://ninja-build.org/) (recommended)
* **Libraries**:
  * **Boost** 1.71.0+ (specifically system, thread, and header-only Beast/Asio)
  * **OpenSSL** 3.0.0+ (needed for secure WebSocket TLS connections)
  * **nlohmann-json** 3.7.3+ (modern JSON parser)

On Debian/Ubuntu-based systems, you can install the main prerequisites via:
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build libboost-all-dev libssl-dev nlohmann-json3-dev
```

### Compiling the Project

This project leverages standard **CMake Presets** to simplify configure and build steps across various environments.

1. **Configure the Project** (using the `grab` preset):
   ```bash
   cmake --preset grab
   ```

2. **Build the Engine**:
   ```bash
   cmake --build --preset grab
   ```

Upon successful compilation, the compiled executable will be located in the binary directory under:
```
.builds/grab/bin/Grab
```

---

## 💻 Running the Engine

Simply execute the compiled binary to start monitoring the market in real-time.

```bash
./.builds/grab/bin/Grab
```

### Real-Time Console Interface
When running, the application will:
1. Initialize the 14 vertices of the price graph.
2. Establish separate parallel TLS WebSockets multiplexing data streams for all active trading pairs.
3. Keep updating the consolidated $14 \times 14$ bid/ask price matrix in real-time.
4. Scan the graph every 5 seconds to prevent rate limits and display detailed logs if an arbitrage opportunity exceeding the commission threshold (e.g., $0.1\%$ per leg) is found.

#### Example Output
```
*** ARBITRAGE OPPORTUNITY DETECTED ***
Path: USDT -> BTC -> ETH -> USDT
Prices: 95420.50000 -> 0.0614500 -> 5863.20000
Calculated Profit: 0.5420%
Estimated Profit (Net of Fees): 0.2420%
Opportunities found: 12
Max profit: 1.1042%
Accumulated profit: 5.4312%
*****************************************
```

---

## 🏗️ Core Class Architecture

### `grab::connection::Connection`
The networking engine. It manages a raw TCP socket, handles SSL handshake negotiations with Binance's server via OpenSSL, and upgrades the session into an active RFC 6455 WebSocket.
* **Auto-Reconnection**: Re-establishes broken networks using configurable exponential backoff strategies.
* **Ping/Pong Heartbeats**: Regularly digests WebSocket ping frames sent by Binance and answers back with pONGs, maintaining connection longevity.
* **24-Hour Cycle**: Explicitly schedules teardown and reinstantiation timers to conform with Binance’s strict connection age constraints.

### `grab::ticker::Ticker`
Acts as a logical consumer wrapper for stream subscriptions.
* Multiplexes stream endpoints (e.g., `<pair>@ticker`) under a shared secure Connection to decrease connection overhead.
* Connects data signals to targeted lambda callbacks, isolating business logic from low-level frame decoding.

### `grab::graph::Graph`
The main data structures. Maps symbols to discrete indexes, computes direct/indirect edges dynamically, and performs cycle searches.
* **`find_negative_cycle_bellman_ford()`**: Relaxes all $E$ edges up to $V-1$ times. In the $V$-th iteration, if a further relaxation occurs, a negative cycle is traced back and reconstructed using a predecessor traversal list.
* **`calculate_arbitrage_profit()`**: Back-calculates cumulative compounded percentage yield over paths to ensure the math checks out perfectly before emitting signals.

---

## 🛡️ License

This project is licensed under the **MIT License**. Feel free to use, modify, and distribute it for private or commercial purposes.

