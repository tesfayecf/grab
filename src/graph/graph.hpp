#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace grab {
namespace graph {

struct Edge {
    int to;
    double weight;
    std::string symbol_pair;
    
    Edge(int to, double weight, const std::string& symbol_pair = "") 
        : to(to), weight(weight), symbol_pair(symbol_pair) {}
};

struct Node {
    std::string label;
    std::vector<Edge> edges;
    
    Node(const std::string& label) : label(label) {}
};

class Graph {
private:
    int num_vertices_;
    std::vector<std::vector<Edge>> adj_list_;
    std::vector<std::string> vertex_labels_;
    std::unordered_map<std::string, int> label_to_index_;
    
public:
    Graph(int num_vertices);
    Graph(const std::vector<std::string>& vertex_labels);
    
    // Basic graph operations
    void add_edge(int from, int to, double weight, const std::string& symbol_pair = "");
    void add_edge(const std::string& from_label, const std::string& to_label, double weight, const std::string& symbol_pair = "");
    
    // Matrix-based operations
    void build_from_price_matrix(const std::vector<double>& prices_matrix, 
                                const std::vector<std::string>& symbols,
                                int rows, int cols);
    
    void update_edge_weight(int from, int to, double new_weight);
    void update_edge_weight(const std::string& from_label, const std::string& to_label, double new_weight);
    
    // Arbitrage detection
    std::vector<int> find_negative_cycle_bellman_ford(int start_vertex = 0);
    std::vector<std::string> find_arbitrage_opportunity();
    double calculate_arbitrage_profit(const std::vector<int>& cycle);
    
    // Utility functions
    void print_graph() const;
    void print_adjacency_matrix() const;
    int get_vertex_index(const std::string& label) const;
    std::string get_vertex_label(int index) const;
    int get_num_vertices() const { return num_vertices_; }
    
    // Get graph data
    const std::vector<std::vector<Edge>>& get_adjacency_list() const { return adj_list_; }
    const std::vector<std::string>& get_vertex_labels() const { return vertex_labels_; }
};

} // namespace graph
} // namespace grab