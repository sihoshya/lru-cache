#pragma once
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>

template <typename K, typename V>
class LruCache {

    struct Node {
        K key{};
        V value{};
        long long expiresAtMs = 0;
        Node* prev = nullptr;
        Node* next = nullptr;
    };
public: 
    using Clock = std::function<long long()>;
    struct Stats {
        long long hits = 0;
        long long misses = 0;
        long long evictions = 0;
        long long expirations = 0;
    };
    static long long systemNowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    explicit LruCache(int capacity, Clock clock = systemNowMs)
        : capacity_(capacity), clock_(std::move(clock)) {
            if (capacity <= 0) throw std::invalid_argument("capacity must be positive");
            head_.next = &tail_;
            tail_.prev=&head_;
        }
    
        ~LruCache() {
            Node* cur = head_.next;
            while (cur != &tail_) {
                Node* nxt = cur->next;
                delete cur;
                cur = nxt;
            }
        }
        LruCache(const LruCache&) = delete;
        LruCache& operator=(const LruCache&) = delete;
        std::optional<V> get(const K& key) {
            std::lock_guard<std::mutex> lock(mu_);
            auto it = map_.find(key);
            if (it == map_.end()) { ++stats_.misses; return std::nullopt; }
            Node* n = it->second;
            if(isExpired(n)) {
                eraseNode(n);
                ++stats_.expirations; ++stats_.misses;
                return std::nullopt;
            }
        moveToFront(n);
        ++stats_.hits;
        return n->value;
    }

    void put(const K& key, V value, long long ttlMs = 0) {
        std::lock_guard<std::mutex> lock(mu_);
        const long long expires = ttlMs > 0 ? clock_() + ttlMs : 0;

        auto it = map_.find(key);
        if(it != map_.end()) {
            Node* n = it->second;
            n->value = std::move(value);
            n->expiresAtMs = expires;
            moveToFront(n);
            return;
        }
        if (static_cast<int>(map_.size()) >= capacity_) {
            removeLast();
            ++stats_.evictions;
        }
        Node* n = new Node();
        n->key = key;
        n->value = std::move(value);
        n->expiresAtMs = expires;
        insertAfterHead(n);
        map_[key] = n;
    }

    bool erase(const K& key) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = map_.find(key);
        if(it == map_.end()) return false;
        eraseNode(it->second);
        return true;
    }

    int size() const {
        std::lock_guard<std::mutex> lock(mu_);
        return static_cast<int>(map_.size());
    }

private: 
    bool isExpired(const Node* n) const {
        return n->expiresAtMs != 0 && clock_() >= n->expiresAtMs;
    }

    void unlink(Node* n) {
        n->prev->next = n->next;
        n->next->prev = n->prev;
    }

    void insertAfterHead(Node* n) {
        n->next = head_.next;
        n->prev = &head_;
        head_.next->prev = n;
        head_.next = n;
    }
    
    void moveToFront(Node* n){ 
        unlink(n); 
        insertAfterHead(n);
    }

    void eraseNode(Node* n) {
        unlink(n);
        map_.erase(n->key);
        delete n;
    }

    void removeLast() { 
        eraseNode(tail_.prev);
    }

    int capacity_;
    Clock clock_;
    std::unordered_map<K, Node*> map_;
    Node head_, tail_;
    mutable std::mutex mu_;
    Stats stats_;
public:
    Stats stats() const {
        std::lock_guard<std::mutex> lock(mu_);
        return stats_;
    }
};


