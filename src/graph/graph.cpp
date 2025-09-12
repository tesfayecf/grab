#include "graph.hpp"
#include <algorithm>
#include <iomanip>
#include <queue>

namespace grab {
namespace graph {

Graph::Graph(int num_vertices) 
    : num_vertices_(num_vertices), adj_list_(num_vertices) {
    // Initialize with default vertex labels
    for (int i = 0; i < num_vertices; ++i) {
        vertex_labels_.push_back("V" + std::to_string(i));
        label_to_index_[vertex_labels_[i]] = i;
    }
}

Graph::Graph(const std::vector<std::string>& vertex_labels) 
    : num_vertices_(vertex_labels.size()), adj_list_(vertex_labels.size()), vertex_labels_(vertex_labels) {
    // Build label to index mapping
    for (int i = 0; i < num_vertices_; ++i) {
        label_to_index_[vertex_labels_[i]] = i;
    }
}

void Graph::add_edge(int from, int to, double weight, const std::string& symbol_pair) {
    if (from >= 0 && from < num_vertices_ && to >= 0 && to < num_vertices_) {
        adj_list_[from].emplace_back(to, weight, symbol_pair);
    } else {
        std::cerr << "Invalid vertex indices: " << from << " -> " << to << std::endl;
    }
}

void Graph::add_edge(const std::string& from_label, const std::string& to_label, double weight, const std::string& symbol_pair) {
    int from_idx = get_vertex_index(from_label);
    int to_idx = get_vertex_index(to_label);
    
    if (from_idx != -1 && to_idx != -1) {
        add_edge(from_idx, to_idx, weight, symbol_pair);
    } else {
        std::cerr << "Invalid vertex labels: " << from_label << " -> " << to_label << std::endl;
    }
}

void Graph::build_from_price_matrix(const std::vector<double>& prices_matrix, 
                                   const std::vector<std::string>& symbols,
                                   int rows, int cols) {
    // Clear existing graph
    adj_list_.clear();
    adj_list_.resize(symbols.size());
    
    num_vertices_ = symbols.size();
    vertex_labels_ = symbols;
    label_to_index_.clear();
    
    // Build label to index mapping
    for (int i = 0; i < num_vertices_; ++i) {
        label_to_index_[vertex_labels_[i]] = i;
    }
    
    // Build edges from price matrix
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (i != j) {  // No self-loops
                double price = prices_matrix[i * cols + j];
                if (price > 0.0) {  // Only add edges for valid prices
                    // Convert price to logarithmic weight for arbitrage detection
                    // Negative log because we want to find cycles with product > 1
                    double log_weight = -std::log(price);
                    std::string symbol_pair = symbols[i] + "->" + symbols[j];
                    add_edge(i, j, log_weight, symbol_pair);
                }
            }
        }
    }
}

void Graph::update_edge_weight(int from, int to, double new_weight) {
    if (from >= 0 && from < num_vertices_) {
        for (auto& edge : adj_list_[from]) {
            if (edge.to == to) {
                edge.weight = new_weight;
                return;
            }
        }
        // If edge doesn't exist, add it
        add_edge(from, to, new_weight);
    }
}

void Graph::update_edge_weight(const std::string& from_label, const std::string& to_label, double new_weight) {
    int from_idx = get_vertex_index(from_label);
    int to_idx = get_vertex_index(to_label);
    
    if (from_idx != -1 && to_idx != -1) {
        update_edge_weight(from_idx, to_idx, new_weight);
    }
}

std::vector<int> Graph::find_negative_cycle_bellman_ford(int start_vertex) {
    std::vector<double> dist(num_vertices_, std::numeric_limits<double>::infinity());
    std::vector<int> parent(num_vertices_, -1);
    
    dist[start_vertex] = 0.0;
    
    // Relax edges V-1 times
    for (int i = 0; i < num_vertices_ - 1; ++i) {
        for (int u = 0; u < num_vertices_; ++u) {
            if (dist[u] != std::numeric_limits<double>::infinity()) {
                for (const auto& edge : adj_list_[u]) {
                    int v = edge.to;
                    double weight = edge.weight;
                    
                    if (dist[u] + weight < dist[v]) {
                        dist[v] = dist[u] + weight;
                        parent[v] = u;
                    }
                }
            }
        }
    }
    
    // Check for negative cycles
    for (int u = 0; u < num_vertices_; ++u) {
        if (dist[u] != std::numeric_limits<double>::infinity()) {
            for (const auto& edge : adj_list_[u]) {
                int v = edge.to;
                double weight = edge.weight;
                
                if (dist[u] + weight < dist[v]) {
                    // Found negative cycle, trace it back
                    std::vector<int> cycle;
                    std::vector<bool> in_cycle(num_vertices_, false);
                    
                    // Move v forward V times to ensure we're in the cycle
                    for (int i = 0; i < num_vertices_; ++i) {
                        v = parent[v];
                    }
                    
                    // Build the cycle
                    int current = v;
                    do {
                        cycle.push_back(current);
                        in_cycle[current] = true;
                        current = parent[current];
                    } while (current != v);
                    
                    std::reverse(cycle.begin(), cycle.end());
                    return cycle;
                }
            }
        }
    }
    
    return {}; // No negative cycle found
}

std::vector<std::string> Graph::find_arbitrage_opportunity() {
    std::vector<int> cycle = find_negative_cycle_bellman_ford(0);
    std::vector<std::string> arbitrage_path;
    
    if (!cycle.empty()) {
        for (int vertex : cycle) {
            arbitrage_path.push_back(vertex_labels_[vertex]);
        }
        // Add the first vertex again to complete the cycle
        arbitrage_path.push_back(vertex_labels_[cycle[0]]);
    }
    
    return arbitrage_path;
}

double Graph::calculate_arbitrage_profit(const std::vector<int>& cycle) {
    if (cycle.empty()) return 0.0;
    
    double total_log_weight = 0.0;
    
    for (size_t i = 0; i < cycle.size(); ++i) {
        int from = cycle[i];
        int to = cycle[(i + 1) % cycle.size()];
        
        // Find the edge weight
        bool found = false;
        for (const auto& edge : adj_list_[from]) {
            if (edge.to == to) {
                total_log_weight += edge.weight;
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cerr << "Edge not found in cycle: " << from << " -> " << to << std::endl;
            return 0.0;
        }
    }
    
    // Convert back from log space
    // If total_log_weight < 0, then we have arbitrage
    double profit_multiplier = std::exp(-total_log_weight);
    return (profit_multiplier - 1.0) * 100.0; // Return percentage profit
}

void Graph::print_graph() const {
    std::cout << "\n=== Graph Structure ===" << std::endl;
    for (int i = 0; i < num_vertices_; ++i) {
        std::cout << "Vertex " << i << " (" << vertex_labels_[i] << "): ";
        for (const auto& edge : adj_list_[i]) {
            std::cout << "-> " << edge.to << "(" << vertex_labels_[edge.to] << ") ";
            std::cout << "[w=" << std::fixed << std::setprecision(6) << edge.weight;
            if (!edge.symbol_pair.empty()) {
                std::cout << ", " << edge.symbol_pair;
            }
            std::cout << "] ";
        }
        std::cout << std::endl;
    }
}

void Graph::print_adjacency_matrix() const {
    std::cout << "\n=== Adjacency Matrix (Weights) ===" << std::endl;
    
    const int width = 12;
    
    // Print header
    std::cout << std::setw(width) << " ";
    for (int j = 0; j < num_vertices_; ++j) {
        std::cout << std::setw(width) << vertex_labels_[j];
    }
    std::cout << std::endl;
    
    // Print separator
    std::cout << std::setw(width) << " ";
    for (int j = 0; j < num_vertices_; ++j) {
        std::cout << std::setw(width) << "------------";
    }
    std::cout << std::endl;
    
    // Print matrix
    for (int i = 0; i < num_vertices_; ++i) {
        std::cout << std::setw(width) << vertex_labels_[i];
        
        for (int j = 0; j < num_vertices_; ++j) {
            double weight = std::numeric_limits<double>::infinity();
            
            if (i == j) {
                weight = 0.0;
            } else {
                // Find edge weight
                for (const auto& edge : adj_list_[i]) {
                    if (edge.to == j) {
                        weight = edge.weight;
                        break;
                    }
                }
            }
            
            if (weight == std::numeric_limits<double>::infinity()) {
                std::cout << std::setw(width) << "INF";
            } else {
                std::cout << std::setw(width) << std::fixed << std::setprecision(4) << weight;
            }
        }
        std::cout << std::endl;
    }
}

int Graph::get_vertex_index(const std::string& label) const {
    auto it = label_to_index_.find(label);
    return (it != label_to_index_.end()) ? it->second : -1;
}

std::string Graph::get_vertex_label(int index) const {
    return (index >= 0 && index < num_vertices_) ? vertex_labels_[index] : "";
}

} // namespace graph
} // namespace grab