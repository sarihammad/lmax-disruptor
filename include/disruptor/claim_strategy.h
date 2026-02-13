#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "disruptor/sequence.h"

namespace disruptor {

class ClaimStrategy {
public:
    virtual ~ClaimStrategy() = default;

    virtual int64_t next(int n = 1) = 0;

    virtual bool hasAvailableCapacity(int required_capacity,
                                      std::vector<Sequence*>& dependents) = 0;
};

class SingleThreadedClaimStrategy : public ClaimStrategy {
public:
    explicit SingleThreadedClaimStrategy(size_t buffer_size)
        : buffer_size_(buffer_size) {}

    int64_t next(int n = 1) override {
        next_value_ += n;
        return next_value_;
    }

    bool hasAvailableCapacity(int required_capacity,
                              std::vector<Sequence*>& dependents) override {
        int64_t wrap_point = next_value_ + required_capacity - buffer_size_;

        if (wrap_point > cached_value_) {
            int64_t min_sequence = getMinimumSequence(dependents);
            cached_value_ = min_sequence;

            if (wrap_point > min_sequence) {
                return false;
            }
        }

        return true;
    }

    int64_t getCurrent() const { return next_value_; }

private:
    int64_t getMinimumSequence(std::vector<Sequence*>& dependents) {
        int64_t minimum = std::numeric_limits<int64_t>::max();
        for (auto* seq : dependents) {
            int64_t value = seq->get();
            if (value < minimum) {
                minimum = value;
            }
        }
        return minimum;
    }

    const size_t buffer_size_;
    Sequence sequence_{-1};
    int64_t next_value_ = -1;
    int64_t cached_value_ = -1;
};

class MultiThreadedClaimStrategy : public ClaimStrategy {
public:
    explicit MultiThreadedClaimStrategy(size_t buffer_size)
        : buffer_size_(buffer_size) {}

    int64_t next(int n = 1) override {
        return sequence_.addAndGet(n);
    }

    bool hasAvailableCapacity(int required_capacity,
                              std::vector<Sequence*>& dependents) override {
        int64_t current = sequence_.get();
        int64_t wrap_point = current + required_capacity - buffer_size_;

        int64_t min_sequence = getMinimumSequence(dependents);
        return wrap_point <= min_sequence;
    }

private:
    int64_t getMinimumSequence(std::vector<Sequence*>& dependents) {
        int64_t minimum = std::numeric_limits<int64_t>::max();
        for (auto* seq : dependents) {
            int64_t value = seq->get();
            if (value < minimum) {
                minimum = value;
            }
        }
        return minimum;
    }

    const size_t buffer_size_;
    Sequence sequence_{-1};
};

} // namespace disruptor
