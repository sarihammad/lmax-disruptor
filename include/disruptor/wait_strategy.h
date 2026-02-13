#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include "disruptor/sequence.h"
#include "disruptor/sequence_group.h"

namespace disruptor {

class WaitStrategy {
public:
    virtual ~WaitStrategy() = default;

    virtual int64_t waitFor(int64_t sequence,
                            Sequence* cursor,
                            std::vector<Sequence*>& dependents) = 0;

    virtual void signalAllWhenBlocking() {}
};

class BusySpinWaitStrategy : public WaitStrategy {
public:
    int64_t waitFor(int64_t sequence,
                    Sequence* cursor,
                    std::vector<Sequence*>& dependents) override {
        while (true) {
            int64_t available = getMinimumSequence(cursor, dependents);
            if (available >= sequence) {
                return available;
            }
            // This fence does not establish synchronization; it simply reduces
            // aggressive loop optimizations on some architectures.
            std::atomic_thread_fence(std::memory_order_acquire);
        }
    }
};

class YieldingWaitStrategy : public WaitStrategy {
public:
    int64_t waitFor(int64_t sequence,
                    Sequence* cursor,
                    std::vector<Sequence*>& dependents) override {
        int spin_tries = 0;
        while (true) {
            int64_t available = getMinimumSequence(cursor, dependents);
            if (available >= sequence) {
                return available;
            }

            if (++spin_tries > 100) {
                std::this_thread::yield();
                spin_tries = 0;
            }
        }
    }
};

} // namespace disruptor
