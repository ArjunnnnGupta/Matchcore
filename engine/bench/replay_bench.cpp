// Replay driver + latency/throughput benchmark harness.
//
// Reads a CSV order file, feeds each order through MatchingEngine::submit(),
// and times each submit() call with steady_clock. Percentiles are computed
// after the run from a pre-reserved latency vector — nothing is printed
// per-order inside the timed loop, since I/O would dominate and lie about
// the engine's real speed (see docs/ARCHITECTURE.md section 2.6).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "FeedHandler.hpp"
#include "MatchingEngine.hpp"
#include "ObjectPool.hpp"

namespace {

struct Args {
    std::string input;
    std::size_t max_orders = 1'000'000;
};

Args parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            args.input = argv[++i];
        } else if (arg == "--orders" && i + 1 < argc) {
            args.max_orders = std::stoull(argv[++i]);
        }
    }
    return args;
}

double percentile(std::vector<uint64_t>& sorted_ns, double p) {
    if (sorted_ns.empty()) return 0.0;
    const double rank = p * static_cast<double>(sorted_ns.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(rank);
    const std::size_t hi = std::min(lo + 1, sorted_ns.size() - 1);
    const double frac = rank - static_cast<double>(lo);
    return static_cast<double>(sorted_ns[lo]) * (1.0 - frac) + static_cast<double>(sorted_ns[hi]) * frac;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace matchcore;
    using clock = std::chrono::steady_clock;

    Args args = parse_args(argc, argv);
    if (args.input.empty()) {
        std::cerr << "usage: matchcore_bench --input <orders.csv> [--orders N]\n";
        return 1;
    }

    std::ifstream file(args.input);
    if (!file) {
        std::cerr << "failed to open input file: " << args.input << "\n";
        return 1;
    }

    ObjectPool<Order> pool(args.max_orders);
    std::size_t trade_count = 0;
    MatchingEngine engine([&trade_count](const Trade&) { ++trade_count; });

    std::vector<uint64_t> latencies_ns;
    latencies_ns.reserve(args.max_orders);

    std::string line;
    std::size_t submitted = 0;
    const auto wall_start = clock::now();

    while (submitted < args.max_orders && std::getline(file, line)) {
        Order* order = pool.acquire();
        if (order == nullptr) break;  // pool exhausted before file did

        if (!FeedHandler::parse_line(line, *order)) {
            pool.release(order);
            continue;  // blank line or header row
        }

        const auto t0 = clock::now();
        engine.submit(order);
        const auto t1 = clock::now();

        latencies_ns.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        ++submitted;
    }

    const auto wall_end = clock::now();
    const double wall_seconds = std::chrono::duration<double>(wall_end - wall_start).count();

    std::sort(latencies_ns.begin(), latencies_ns.end());

    std::printf("orders submitted:  %zu\n", submitted);
    std::printf("trades executed:   %zu\n", trade_count);
    std::printf("wall time:         %.6f s\n", wall_seconds);
    std::printf("throughput:        %.0f orders/sec\n",
                 wall_seconds > 0.0 ? static_cast<double>(submitted) / wall_seconds : 0.0);
    if (!latencies_ns.empty()) {
        std::printf("latency p50:       %.0f ns\n", percentile(latencies_ns, 0.50));
        std::printf("latency p90:       %.0f ns\n", percentile(latencies_ns, 0.90));
        std::printf("latency p99:       %.0f ns\n", percentile(latencies_ns, 0.99));
        std::printf("latency p99.9:     %.0f ns\n", percentile(latencies_ns, 0.999));
        std::printf("latency max:       %llu ns\n",
                     static_cast<unsigned long long>(latencies_ns.back()));
    }

    return 0;
}
