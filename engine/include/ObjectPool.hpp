#pragma once

#include <vector>

namespace matchcore {

// Fixed-capacity freelist pool. All storage is allocated once upfront in the
// constructor; acquire()/release() do no heap allocation, which is the point
// (see docs/ARCHITECTURE.md section 2.5 — removing new/delete from the
// matching hot path). acquire() returns nullptr once the pool is exhausted;
// callers decide the exhaustion policy rather than the pool silently
// growing, since silent growth would reintroduce the allocation this exists
// to avoid.
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(std::size_t capacity) : storage_(capacity) {
        free_list_.reserve(capacity);
        for (std::size_t i = capacity; i-- > 0;) {
            free_list_.push_back(&storage_[i]);
        }
    }

    T* acquire() {
        if (free_list_.empty()) return nullptr;
        T* obj = free_list_.back();
        free_list_.pop_back();
        return obj;
    }

    void release(T* obj) { free_list_.push_back(obj); }

    std::size_t capacity() const { return storage_.size(); }
    std::size_t available() const { return free_list_.size(); }

private:
    std::vector<T> storage_;
    std::vector<T*> free_list_;
};

}  // namespace matchcore
