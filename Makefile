CXX      ?= clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -g
THREADS  := -pthread

.PHONY: test test-asan test-tsan bench clean

# Default: run the tests under AddressSanitizer + UBSan (catches leaks,
# use-after-free, out-of-bounds, undefined behaviour).
test: test-asan

test-asan:
	$(CXX) $(CXXFLAGS) $(THREADS) -fsanitize=address,undefined test_lru.cpp -o test_lru_asan
	./test_lru_asan

# ThreadSanitizer catches data races. Cannot be combined with AddressSanitizer.
test-tsan:
	$(CXX) $(CXXFLAGS) $(THREADS) -fsanitize=thread test_lru.cpp -o test_lru_tsan
	./test_lru_tsan

bench:
	$(CXX) -std=c++17 -O2 $(THREADS) bench.cpp -o bench
	./bench

clean:
	rm -f test_lru_asan test_lru_tsan bench
