// bench.cpp -- measures operations per second at 1 thread vs 8 threads.
// Build with optimizations (no sanitizers) for meaningful numbers: make bench

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "lru_cache.hpp"

static double run(int threads, int opsPerThread) {
    LruCache<int, int> cache(10000);
    for (int i = 0; i < 10000; ++i) cache.put(i, i);  // warm up

    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            std::mt19937 rng(t + 1);
            for (int i = 0; i < opsPerThread; ++i) {
                int key = static_cast<int>(rng() % 20000);  // ~50% hit rate
                if (rng() % 10 == 0) cache.put(key, key);   // 10% writes
                else cache.get(key);
            }
        });
    }
    for (auto& th : pool) th.join();
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return (static_cast<double>(threads) * opsPerThread) / secs;
}

int main() {
    const int total = 2000000;  // same total work at every thread count
    std::cout << std::fixed << std::setprecision(0);
    for (int threads : {1, 2, 4, 8}) {
        double ops = run(threads, total / threads);
        std::cout << threads << " thread(s): " << ops << " ops/sec\n";
    }
    std::cout << "(One global lock: more threads add contention, not speed.\n"
              << " Sharding the cache into N independent caches is the fix.)\n";
    return 0;
}
