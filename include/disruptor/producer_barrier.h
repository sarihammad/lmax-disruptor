#pragma once

#include <thread>
#include <vector>

#include "disruptor/claim_strategy.h"
#include "disruptor/ring_buffer.h"
#include "disruptor/sequence.h"

namespace disruptor {

template <typename T, typename EntryFactory = DefaultEntryFactory<T>>
class ProducerBarrier {
public:
    ProducerBarrier(RingBuffer<T, EntryFactory>* ring_buffer,
                    ClaimStrategy* claim_strategy,
                    std::vector<Sequence*> gating_sequences)
        : ring_buffer_(ring_buffer)
        , claim_strategy_(claim_strategy)
        , gating_sequences_(std::move(gating_sequences)) {}

    int64_t nextEntry() {
        return nextEntry(1);
    }

    int64_t nextEntry(int n) {
        while (!claim_strategy_->hasAvailableCapacity(n, gating_sequences_)) {
            std::this_thread::yield();
        }
        return claim_strategy_->next(n);
    }

    T& getEntry(int64_t sequence) {
        ring_buffer_->prepareForWrite(sequence);
        return ring_buffer_->get(sequence);
    }

    void commit(int64_t sequence) {
        ring_buffer_->publish(sequence);
    }

    void commit(int64_t lo, int64_t hi) {
        ring_buffer_->publish(lo, hi);
    }

private:
    RingBuffer<T, EntryFactory>* ring_buffer_;
    ClaimStrategy* claim_strategy_;
    std::vector<Sequence*> gating_sequences_;
};

} // namespace disruptor
