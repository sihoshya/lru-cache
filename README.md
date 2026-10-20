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

Fill in from `make bench` on your machine, for example:

| Threads | ops/sec |
|---------|---------|
| 1       | [fill in] |
| 8       | [fill in] |

With one global lock, more threads add contention instead of speed.

## Next steps

- Shard into N independent caches (hash the key to pick one), each with its own lock, and re-run the benchmark.
- Add a `std::list`-based version and compare.
