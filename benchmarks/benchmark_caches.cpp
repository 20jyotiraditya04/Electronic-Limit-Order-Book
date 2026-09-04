//
// Created by martian_duck on 29/08/26.
//


#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <memory>
#include <iomanip>
#include "L2Snapshot.hpp"
#include "RedisCacheClient.hpp"
#include "MemcachedCacheClient.hpp"



struct BenchmarkResult {
    std::string cache_name;
    double throughput_ops_sec{0.0};
    double p50_us{0.0};
    double p90_us{0.0};
    double p99_us{0.0};
    double p999_us{0.0};
    double max_us{0.0};
};

template <typename ClientFactory>
BenchmarkResult run_benchmark(const std::string& name, ClientFactory factory,
                              size_t total_ops, size_t num_threads) {

    L2snapshot<5> snapshot;
    snapshot.bid_count = 2;
    snapshot.bids[0] = LevelQuote{10045, 50, 1};
    snapshot.bids[1] = LevelQuote{10040, 30, 2};
    snapshot.ask_count = 2;
    snapshot.asks[0] = LevelQuote{10050, 50, 2};
    snapshot.asks[1] = LevelQuote{10055, 80, 1};

    char payload_buf[256];
    size_t len = snapshot.serialize_to_buffer(payload_buf, sizeof(payload_buf));
    std::string payload(payload_buf, len);

    size_t ops_per_thread = total_ops / num_threads;
    std::vector<std::vector<double>> thread_latencies(num_threads);
    std::vector<std::thread> workers;

    auto start_total = std::chrono::steady_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        workers.emplace_back([t, ops_per_thread, &factory, &payload, &thread_latencies]() {
            auto client = factory();
            if (!client->connect_to_server()) {
                std::cerr << "Thread " << t << " failed to connect!\n";
                return;
            }

            thread_latencies[t].reserve(ops_per_thread);
            std::string key = "L2_SYM_" + std::to_string(t);

            for (size_t i = 0; i < ops_per_thread; ++i) {
                auto t0 = std::chrono::steady_clock::now();
                client->set(key, payload);
                auto t1 = std::chrono::steady_clock::now();

                double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                thread_latencies[t].push_back(elapsed_us);
            }
            client->disconnect();
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    auto end_total = std::chrono::steady_clock::now();
    double total_sec = std::chrono::duration<double>(end_total - start_total).count();

    std::vector<double> all_latencies;
    all_latencies.reserve(total_ops);
    for (const auto& tl : thread_latencies) {
        all_latencies.insert(all_latencies.end(), tl.begin(), tl.end());
    }

    std::sort(all_latencies.begin(), all_latencies.end());

    BenchmarkResult res;
    res.cache_name = name;
    res.throughput_ops_sec = static_cast<double>(all_latencies.size()) / total_sec;
    if (!all_latencies.empty()) {
        res.p50_us  = all_latencies[static_cast<size_t>(all_latencies.size() * 0.50)];
        res.p90_us  = all_latencies[static_cast<size_t>(all_latencies.size() * 0.90)];
        res.p99_us  = all_latencies[static_cast<size_t>(all_latencies.size() * 0.99)];
        res.p999_us = all_latencies[static_cast<size_t>(all_latencies.size() * 0.999)];
        res.max_us  = all_latencies.back();
    }
    return res;
}

void print_results(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n" << std::string(85, '=') << "\n";
    std::cout << std::left << std::setw(15) << "Engine"
              << std::right << std::setw(16) << "Throughput (ops/s)"
              << std::setw(12) << "p50 (us)"
              << std::setw(12) << "p90 (us)"
              << std::setw(12) << "p99 (us)"
              << std::setw(14) << "p99.9 (us)"
              << "\n";
    std::cout << std::string(85, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(15) << r.cache_name
                  << std::right << std::setw(16) << std::fixed << std::setprecision(0) << r.throughput_ops_sec
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.p50_us
                  << std::setw(12) << r.p90_us
                  << std::setw(12) << r.p99_us
                  << std::setw(14) << r.p999_us
                  << "\n";
    }
    std::cout << std::string(85, '=') << "\n\n";
}

int main() {
    const size_t TOTAL_OPS = 50000;
    const size_t THREADS = 8;

    std::cout << "Starting In-Memory State Cache Benchmark...\n";
    std::cout << "Configuration: " << TOTAL_OPS << " Total Operations across " << THREADS << " Concurrent Threads\n";

    std::vector<BenchmarkResult> results;

    // 1. Valkey / Redis (Port 6379)
    std::cout << "\n--> Benchmarking Valkey/Redis (Port 6379)..." << std::flush;
    results.push_back(run_benchmark("Valkey/Redis", []() {
        return std::make_unique<RedisCacheClient>("127.0.0.1", 6379, "Valkey/Redis");
    }, TOTAL_OPS, THREADS));
    std::cout << " Done.";

    // 2. Dragonfly (Port 6380)
    std::cout << "\n--> Benchmarking Dragonfly (Port 6380)..." << std::flush;
    results.push_back(run_benchmark("Dragonfly", []() {
        return std::make_unique<RedisCacheClient>("127.0.0.1", 6380, "Dragonfly");
    }, TOTAL_OPS, THREADS));
    std::cout << " Done.";

    // 3. Memcached (Port 11211)
    std::cout << "\n--> Benchmarking Memcached (Port 11211)..." << std::flush;
    results.push_back(run_benchmark("Memcached", []() {
        return std::make_unique<MemcachedCacheClient>("127.0.0.1", 11211);
    }, TOTAL_OPS, THREADS));
    std::cout << " Done.\n";

    print_results(results);
    return 0;
}