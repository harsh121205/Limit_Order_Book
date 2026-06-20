#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <memory>
#include "orderbook.hpp"

struct OrderMsg {
    MsgType type;
    Ticker ticker;
    int id;
    double price;
    int volume;
    bool is_buy;
    bool is_market;
    bool is_ioc;
};

std::vector<OrderMsg> generate_orders(size_t count, unsigned int seed) {
    std::vector<OrderMsg> dataset;
    dataset.reserve(count);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> price_dist(100.00, 105.00);
    std::uniform_int_distribution<int> vol_dist(10, 500);
    std::uniform_int_distribution<int> action_dist(0, 100); 

    int next_id = 0;
    std::vector<int> active_ids;
    active_ids.reserve(count / 2);

    Ticker default_ticker("RELIANCE");

    for (size_t i = 0; i < count; ++i) {
        int action = action_dist(rng);
        bool is_buy = (action_dist(rng) % 2 == 0);
        double price = price_dist(rng);
        int vol = vol_dist(rng);

        if (action < 60 || active_ids.empty()) { 
            // Standard Limit Order
            dataset.push_back({MsgType::ADD, default_ticker, next_id, price, vol, is_buy, false, false});
            active_ids.push_back(next_id);
            ++next_id;
        } 
        else if (action < 85) { 
            // Cancel Order
            size_t idx = action_dist(rng) % active_ids.size();
            int cancel_id = active_ids[idx];
            
            // Swap and pop to remove ID in O(1) time
            active_ids[idx] = active_ids.back();
            active_ids.pop_back();

            dataset.push_back({MsgType::CANCEL, default_ticker, cancel_id, 0.0, 0, false, false, false});
        } 
        else if (action < 95) { 
            // Market Order
            dataset.push_back({MsgType::ADD, default_ticker, next_id, price, vol, is_buy, true, false});
            ++next_id;
        } 
        else { 
            // IOC Order
            dataset.push_back({MsgType::ADD, default_ticker, next_id, price, vol, is_buy, false, true});
            ++next_id;
        }
    }
    return dataset;
}

int main() {
    const size_t warmup_size = 100'000;
    const size_t test_size = 5'000'000;

    std::cout << "Generating test data...\n";
    auto warmup_orders = generate_orders(warmup_size, 42);
    auto test_orders = generate_orders(test_size, 43);

    auto engine = std::make_unique<OrderBook>();

    // Warmup phase to prime the CPU cache
    std::cout << "Running warm-up...\n";
    for (const auto& msg : warmup_orders) {
        if (msg.type == MsgType::ADD) {
            engine->add_order(msg.id, msg.ticker, msg.price, msg.volume, msg.is_buy, msg.is_market, msg.is_ioc);
        } else {
            engine->cancel_order(msg.id);
        }
    }

    std::vector<uint64_t> latencies;
    latencies.reserve(test_size);

    std::cout << "Running benchmark on " << test_size << " orders...\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    for (const auto& msg : test_orders) {
        auto op_start = std::chrono::high_resolution_clock::now();

        if (msg.type == MsgType::ADD) {
            engine->add_order(msg.id, msg.ticker, msg.price, msg.volume, msg.is_buy, msg.is_market, msg.is_ioc);
        } else {
            engine->cancel_order(msg.id);
        }

        auto op_end = std::chrono::high_resolution_clock::now();
        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    // calculate statistics
    uint64_t total_ns = std::accumulate(latencies.begin(), latencies.end(), 0ULL);
    double mean_ns = static_cast<double>(total_ns) / test_size;

    // find percentiles
    auto get_percentile = [&](double p) {
        auto it = latencies.begin() + static_cast<size_t>(test_size * p);
        std::nth_element(latencies.begin(), it, latencies.end());
        return *it;
    };

    uint64_t p50 = get_percentile(0.50);
    uint64_t p99 = get_percentile(0.99);
    uint64_t p999 = get_percentile(0.999);
    uint64_t max_ns = *std::max_element(latencies.begin(), latencies.end());

    double throughput = (test_size / (total_ms / 1000.0)) / 1'000'000.0;

    std::cout << "\n=== BENCHMARK RESULTS ===\n";
    std::cout << "Time        : " << total_ms << " ms\n";
    std::cout << "Throughput  : " << std::fixed << std::setprecision(2) << throughput << " million msgs/sec\n";
    std::cout << "Mean Latency: " << mean_ns << " ns\n";
    std::cout << "Median (p50): " << p50 << " ns\n";
    std::cout << "p99         : " << p99 << " ns\n";
    std::cout << "p99.9       : " << p999 << " ns\n";
    std::cout << "Max Latency : " << max_ns << " ns\n";
    std::cout << "=========================\n";

    return 0;
}