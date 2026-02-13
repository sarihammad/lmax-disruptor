#pragma once

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "disruptor/claim_strategy.h"
#include "disruptor/consumer.h"
#include "disruptor/consumer_barrier.h"
#include "disruptor/producer_barrier.h"
#include "disruptor/ring_buffer.h"
#include "disruptor/wait_strategy.h"

namespace disruptor {

template <typename T, typename EntryFactory = DefaultEntryFactory<T>>
class Disruptor {
public:
    enum class ClaimStrategyType { SINGLE_THREADED, MULTI_THREADED };
    enum class WaitStrategyType { BUSY_SPIN, YIELDING };

    explicit Disruptor(size_t buffer_size,
                       ClaimStrategyType claim_type = ClaimStrategyType::SINGLE_THREADED,
                       WaitStrategyType wait_type = WaitStrategyType::YIELDING,
                       EntryFactory entry_factory = EntryFactory())
        : ring_buffer_(std::make_unique<RingBuffer<T, EntryFactory>>(buffer_size,
                                                                     std::move(entry_factory))) {
        if (claim_type == ClaimStrategyType::SINGLE_THREADED) {
            claim_strategy_ = std::make_unique<SingleThreadedClaimStrategy>(
                ring_buffer_->getBufferSize());
        } else {
            throw std::logic_error(
                "Multi-producer publishing is not fully ordered in this implementation. "
                "Use SINGLE_THREADED claim strategy.");
        }

        if (wait_type == WaitStrategyType::BUSY_SPIN) {
            wait_strategy_ = std::make_unique<BusySpinWaitStrategy>();
        } else {
            wait_strategy_ = std::make_unique<YieldingWaitStrategy>();
        }
    }

    ProducerBarrier<T, EntryFactory>* getProducerBarrier() {
        if (!producer_barrier_) {
            producer_barrier_ = std::make_unique<ProducerBarrier<T, EntryFactory>>(
                ring_buffer_.get(),
                claim_strategy_.get(),
                gating_sequences_);
        }
        return producer_barrier_.get();
    }

    Consumer<T, EntryFactory>* createConsumer(BatchHandler<T>* handler,
                                              std::vector<Sequence*> dependencies = {}) {
        auto consumer_barrier = std::make_unique<ConsumerBarrier<T, EntryFactory>>(
            ring_buffer_.get(),
            wait_strategy_.get(),
            std::move(dependencies));

        auto consumer = std::make_unique<Consumer<T, EntryFactory>>(consumer_barrier.get(),
                                                                    handler);
        gating_sequences_.push_back(consumer->getSequence());

        Consumer<T, EntryFactory>* consumer_ptr = consumer.get();
        consumers_.push_back(std::move(consumer));
        consumer_barriers_.push_back(std::move(consumer_barrier));
        return consumer_ptr;
    }

    void start() {
        for (auto& consumer : consumers_) {
            consumer->start();
        }
    }

    void stop() {
        for (auto& consumer : consumers_) {
            consumer->stop();
        }
    }

    RingBuffer<T, EntryFactory>* getRingBuffer() { return ring_buffer_.get(); }

private:
    std::unique_ptr<RingBuffer<T, EntryFactory>> ring_buffer_;
    std::unique_ptr<ClaimStrategy> claim_strategy_;
    std::unique_ptr<WaitStrategy> wait_strategy_;
    std::unique_ptr<ProducerBarrier<T, EntryFactory>> producer_barrier_;
    std::vector<std::unique_ptr<Consumer<T, EntryFactory>>> consumers_;
    std::vector<std::unique_ptr<ConsumerBarrier<T, EntryFactory>>> consumer_barriers_;
    std::vector<Sequence*> gating_sequences_;
};

} // namespace disruptor
