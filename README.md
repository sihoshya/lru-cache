# Thread-safe LRU Cache with TTL (C++17)

A header-only LRU cache: `get` and `put` are O(1), entries can expire after a TTL, and it is safe to use from many threads.

## Design

- `std::unordered_map<K, Node*>` finds an entry in O(1).
- A doubly linked list orders entries by recency (front = newest, back = next to evict). With a pointer to a node, unlinking and re-inserting it is O(1).
- Two sentinel nodes (`head_`, `tail_`) mean there are no empty-list or end-of-list special cases.
- Lazy TTL: expiry is checked in `get`, so no background thread is needed. A fake clock can be injected so tests never sleep.
- One `std::mutex` guards everything. A read-write lock would not help, because even `get` reorders the list.
- The cache owns its nodes (`new`/`delete`), so copying is deleted to prevent a double free.

## Build and test

```
make test        # tests under AddressSanitizer + UBSan
make test-tsan   # tests under ThreadSanitizer (data race detector)
make bench       # throughput numbers (optimized build)
```

## What the tests cover

Empty cache, capacity 1, overwrite, eviction order, `get` refreshing recency, TTL with a fake clock, stats, bad capacity, 20,000 random operations compared against a simple reference model, and an 8-thread stress test.

## Results

Measured with `make bench` (`-O2`, no sanitizers) on [your Mac model and chip]. Each row runs the same 2,000,000 operations split across the given number of threads, on a cache of capacity 10,000 with keys drawn from 0 to 19,999 (about 50% hit rate) and 10% of operations being writes. Five runs; the table shows the median and the range.

| Threads | ops/sec (median) | Range across 5 runs |
|---------|------------------|---------------------|
| 1       | 33.3 million     | 31.3 to 33.8 million |
| 2       | 14.4 million     | 14.0 to 15.5 million |
| 4       | 10.5 million     | 9.5 to 10.8 million |
| 8       | 6.4 million      | 6.3 to 6.5 million |

Throughput **falls** as threads are added: 8 threads run about 5x slower than 1 thread on the same total work. The cause is the single global mutex. Every `get` and `put` takes it, so threads mostly wait on each other, and handing the lock between cores costs more than the short operations themselves.

## Next steps

- Shard into N independent caches (hash the key to pick one), each with its own lock, and re-run the benchmark.
- Add a `std::list`-based version and compare.
