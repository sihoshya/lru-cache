#include <atomic>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "lru_cache.hpp"

static int g_failures = 0;
#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::cerr <<"FAIL " << __FILE__ << ":" <<__LINE__ << "  " #cond \
                << "\n"; \
            ++g_failures; \
        } \
    } while (0)

struct FakeClock {
    long long now = 1000;
    std::function<long long()> fn() { return [this] { return now; }; }
};

void test_empty_get() {
    LruCache<int, int> c(2);
    CHECK(!c.get(1).has_value());
    CHECK(c.size() == 0);
}


void test_put_then_get() {
    LruCache<int, std::string> c(2);
    c.put(1, "one");
    auto v = c.get(1);
    CHECK(v.has_value() && *v == "one");
}

void test_capacity_one() {
    LruCache<int, int> c(1);
    c.put(1, 10);
    c.put(2, 20);  // evicts 1
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2).value() == 20);
    CHECK(c.size() == 1);
}

void test_overwrite_existing_key() {
    LruCache<int, int> c(2);
    c.put(1, 10);
    c.put(1, 11);
    CHECK(c.size() == 1);
    CHECK(c.get(1).value() == 11);
}

void test_eviction_order() {
    LruCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(3, 3);
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2).has_value());
    CHECK(c.get(3).has_value());
}

void test_get_refreshes_recency() {
    LruCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.get(1);
    c.put(3, 3);
    CHECK(c.get(1).has_value());
    CHECK(!c.get(2).has_value());
    CHECK(c.get(3).has_value());
}

void test_put_overwrite_refreshes_recency() {
    LruCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(1, 100);
    c.put(3, 3);
    CHECK(c.get(1).value() == 100);
    CHECK(!c.get(2).has_value());
}

void test_ttl_expiry() {
    FakeClock clk;
    LruCache<int,int> c(4, clk.fn());
    c.put(1, 10, 500);
    c.put(2, 20);
    clk.now += 499;
    CHECK(c.get(1).has_value());
    clk.now += 1;
    CHECK(!c.get(1).has_value());
    CHECK(c.get(2).has_value());
    CHECK(c.size() == 1);
    CHECK(c.stats().expirations == 1);
}

void test_ttl_reset_on_overwrite() {
    FakeClock clk;
    LruCache<int,int> c(4, clk.fn());
    c.put(1, 10, 100);
    clk.now += 90;
    c.put(1, 11, 100);
    clk.now += 90;
    CHECK(c.get(1).has_value());
}
void test_erase() {
    LruCache<int, int> c(2);
    c.put(1, 1);
    CHECK(c.erase(1));
    CHECK(!c.erase(1));
    CHECK(!c.get(1).has_value());
    CHECK(c.size() == 0);
}

void test_stats() {
    LruCache<int, int> c(1);
    c.put(1, 1);
    c.get(1);
    c.get(2);
    c.put(2, 2);
    auto s = c.stats();
    CHECK(s.hits == 1);
    CHECK(s.misses == 1);
    CHECK(s.evictions == 1);
}

void test_bad_capacity() {
    bool threw = false;
    try {
        LruCache<int, int> c(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

// Compare against a deliberately simple "obviously correct" model on random
// operations. If the fancy O(1) cache ever disagrees with the slow model,
// there is a bug in the pointer code
void test_random_against_model() {
    const int cap = 5;
    LruCache<int, int> c(cap);
    std::vector<std::pair<int, int>> model;
    std::mt19937 rng(12345);
    for (int i = 0; i < 20000; ++i){
        int key = static_cast<int>(rng() % 12);
        if (rng() % 2 == 0) {
            int val = static_cast<int>(rng() % 1000);
            c.put(key, val);
            for (size_t j = 0; j < model.size(); ++j)
                if (model[j].first == key) {
                    model.erase(model.begin() + j);
                    break;
                } 
            model.insert(model.begin(), {key, val});
            if (static_cast<int>(model.size()) > cap) model.pop_back();
        } else {
            auto got = c.get(key);
            int found = -1;
            for (size_t j = 0; j < model.size(); ++j)
                if (model[j].first == key) {
                    found = static_cast<int>(j);
                    break;
                }
            if (found < 0) {
                CHECK(!got.has_value());
            } else {
                auto entry = model[found];
                model.erase(model.begin() + found);
                model.insert(model.begin(), entry);
                CHECK(got.has_value() && *got == entry.second);
            }
        }
        if (g_failures) return;
    }
    CHECK(c.size() == static_cast<int>(model.size()));

}

void test_concurrent_stress() {
    const int threads = 8;
    const int opsPerThread = 50000;
    const int cap = 64;
    LruCache<int, int> c(cap);
    std::atomic<long long> gotValues{0};

    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            std::mt19937 rng(1000 + t);
            for (int i = 0; i < opsPerThread; ++i) {
                int key = static_cast<int>(rng() % 200);
                if (rng() % 3 == 0) {
                    c.put(key, key * 2);
                } else {
                    auto v = c.get(key);
                    if(v) {
                        if(*v != key * 2) std::abort();
                        ++gotValues;
                    }
                }
            }
        });
    }
    for (auto& th : pool) th.join();
    CHECK(c.size() <= cap);
    CHECK(c.size() > 0);
    auto s = c.stats();
    CHECK(s.hits == gotValues.load());
}
int main() {
    test_empty_get();
    test_put_then_get();
    test_capacity_one();
    test_overwrite_existing_key();
    test_eviction_order();
    test_get_refreshes_recency();
    test_put_overwrite_refreshes_recency();
    test_ttl_expiry();
    test_ttl_reset_on_overwrite();
    test_erase();
    test_stats();
    test_bad_capacity();
    test_random_against_model();
    test_concurrent_stress();

    if (g_failures == 0) {
        std::cout << "all LRU tests passed\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
