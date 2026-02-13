#pragma once

#include <vector>

#include "disruptor/ring_buffer.h"
#include "disruptor/sequence.h"
#include "disruptor/wait_strategy.h"

namespace disruptor {

template <typename T, typename EntryFactory = DefaultEntryFactory<T>>
class ConsumerBarrier {
public:
    ConsumerBarrier(RingBuffer<T, EntryFactory>* ring_buffer,
                    WaitStrategy* wait_strategy,
                    std::vector<Sequence*> dependents = {})
        : ring_buffer_(ring_buffer)
        , wait_strategy_(wait_strategy)
        , cursor_(ring_buffer->getCursor())
        , dependent_sequences_(std::move(dependents)) {}

    int64_t waitFor(int64_t sequence) {
        return wait_strategy_->waitFor(sequence, cursor_, dependent_sequences_);
    }

    const T& getEntry(int64_t sequence) const {
        return ring_buffer_->get(sequence);
    }

    T& getEntry(int64_t sequence) {
        return ring_buffer_->get(sequence);
    }

private:
    RingBuffer<T, EntryFactory>* ring_buffer_;
    WaitStrategy* wait_strategy_;
    Sequence* cursor_;
    std::vector<Sequence*> dependent_sequences_;
};

} // namespace disruptor
