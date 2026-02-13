#pragma once

#include <atomic>
#include <thread>

#include "disruptor/batch_handler.h"
#include "disruptor/consumer_barrier.h"
#include "disruptor/sequence.h"

namespace disruptor {

template <typename T, typename EntryFactory = DefaultEntryFactory<T>>
class Consumer {
public:
    Consumer(ConsumerBarrier<T, EntryFactory>* barrier, BatchHandler<T>* handler)
        : barrier_(barrier), handler_(handler) {}

    ~Consumer() {
        stop();
    }

    void start() {
        running_ = true;
        thread_ = std::thread([this]() { run(); });
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    Sequence* getSequence() { return &sequence_; }

private:
    void run() {
        int64_t next_sequence = sequence_.get() + 1;

        while (running_) {
            try {
                int64_t available = barrier_->waitFor(next_sequence);

                while (next_sequence <= available) {
                    T& entry = barrier_->getEntry(next_sequence);
                    bool end_of_batch = (next_sequence == available);

                    handler_->onAvailable(entry, next_sequence, end_of_batch);
                    next_sequence++;
                }

                sequence_.set(available);
            } catch (...) {
                break;
            }
        }

        handler_->onCompletion();
    }

    ConsumerBarrier<T, EntryFactory>* barrier_;
    BatchHandler<T>* handler_;
    Sequence sequence_{-1};
    std::atomic<bool> running_{false};
    std::thread thread_;
};

} // namespace disruptor
