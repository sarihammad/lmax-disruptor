#pragma once

#include <atomic>
#include <cstdint>

namespace disruptor {

class Sequence {
public:
    Sequence() = default;
    explicit Sequence(int64_t initial) : value_(initial) {}

    int64_t get() const {
        return value_.load(std::memory_order_acquire);
    }

    void set(int64_t value) {
        value_.store(value, std::memory_order_release);
    }

    void setVolatile(int64_t value) {
        value_.store(value, std::memory_order_seq_cst);
    }

    bool compareAndSet(int64_t expected, int64_t desired) {
        return value_.compare_exchange_strong(expected, desired,
                                             std::memory_order_release,
                                             std::memory_order_acquire);
    }

    int64_t incrementAndGet() {
        return value_.fetch_add(1, std::memory_order_release) + 1;
    }

    int64_t addAndGet(int64_t increment) {
        return value_.fetch_add(increment, std::memory_order_release) + increment;
    }

    void setMonotonic(int64_t value) {
        int64_t current = value_.load(std::memory_order_acquire);
        while (value > current &&
               !value_.compare_exchange_weak(current, value,
                                            std::memory_order_release,
                                            std::memory_order_acquire)) {
        }
    }

private:
    alignas(64) std::atomic<int64_t> value_{-1};
};

} // namespace disruptor
