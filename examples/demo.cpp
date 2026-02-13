#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "disruptor/disruptor.h"

namespace {

using disruptor::BatchHandler;
using disruptor::Disruptor;

struct Event {
    int64_t value;
    int64_t timestamp;
};

class SimpleHandler : public BatchHandler<Event> {
public:
    void onAvailable(const Event& event, int64_t /*sequence*/, bool /*end_of_batch*/) override {
        count_++;
        last_value_ = event.value;
    }

    int64_t getCount() const { return count_.load(); }
    int64_t getLastValue() const { return last_value_.load(); }

private:
    std::atomic<int64_t> count_{0};
    std::atomic<int64_t> last_value_{0};
};

void runBenchmark() {
    std::cout << "\n=== Disruptor Benchmark (Single Producer) ===\n\n";

    const size_t buffer_size = 1024;
    const int64_t iterations = 100'000;

    Disruptor<Event> disruptor(buffer_size,
                               Disruptor<Event>::ClaimStrategyType::SINGLE_THREADED,
                               Disruptor<Event>::WaitStrategyType::YIELDING);

    SimpleHandler handler;
    auto* consumer = disruptor.createConsumer(&handler);
    auto* producer = disruptor.getProducerBarrier();

    (void)consumer;

    disruptor.start();

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int64_t i = 0; i < iterations; i++) {
        int64_t sequence = producer->nextEntry();
        Event& event = producer->getEntry(sequence);
        event.value = i;
        event.timestamp = std::chrono::high_resolution_clock::now()
                              .time_since_epoch()
                              .count();
        producer->commit(sequence);
    }

    while (handler.getCount() < iterations) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(end_time - start_time).count();

    disruptor.stop();

    std::cout << "Events processed: " << iterations << "\n";
    std::cout << "Time taken: " << duration << " seconds\n";
    std::cout << "Throughput: " << (iterations / duration / 1e6) << " million ops/sec\n";
    std::cout << "Latency per op: " << (duration / iterations * 1e9) << " ns\n";
}

struct PipelineEvent {
    int64_t data;
    int64_t stage1_result;
    int64_t stage2_result;
    int64_t stage3_result;
};

class Stage1Handler : public BatchHandler<PipelineEvent> {
public:
    void onAvailable(const PipelineEvent& event, int64_t /*sequence*/, bool /*eob*/) override {
        const_cast<PipelineEvent&>(event).stage1_result = event.data * 2;
    }
};

class Stage2Handler : public BatchHandler<PipelineEvent> {
public:
    void onAvailable(const PipelineEvent& event, int64_t /*sequence*/, bool /*eob*/) override {
        const_cast<PipelineEvent&>(event).stage2_result = event.stage1_result + 10;
    }
};

class Stage3Handler : public BatchHandler<PipelineEvent> {
public:
    void onAvailable(const PipelineEvent& event, int64_t /*sequence*/, bool /*eob*/) override {
        const_cast<PipelineEvent&>(event).stage3_result = event.stage2_result * 3;
        count_++;
    }

    int64_t getCount() const { return count_.load(); }

private:
    std::atomic<int64_t> count_{0};
};

void runPipelineDemo() {
    std::cout << "\n=== Three-Stage Pipeline Demo ===\n\n";

    const size_t buffer_size = 64;
    const int64_t events = 1000;

    Disruptor<PipelineEvent> disruptor(buffer_size);

    Stage1Handler handler1;
    Stage2Handler handler2;
    Stage3Handler handler3;

    auto* consumer1 = disruptor.createConsumer(&handler1);
    auto* consumer2 = disruptor.createConsumer(&handler2, {consumer1->getSequence()});
    auto* consumer3 = disruptor.createConsumer(&handler3, {consumer2->getSequence()});

    auto* producer = disruptor.getProducerBarrier();

    (void)consumer3;

    disruptor.start();

    for (int64_t i = 0; i < events; i++) {
        int64_t seq = producer->nextEntry();
        PipelineEvent& event = producer->getEntry(seq);
        event.data = i;
        producer->commit(seq);
    }

    while (handler3.getCount() < events) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    disruptor.stop();

    std::cout << "Pipeline processed " << events << " events\n";
    std::cout << "Each event went through 3 stages with dependencies\n";
}

void runSimpleRingBufferDemo() {
    std::cout << "\n=== Ring Buffer Demo ===\n\n";

    Disruptor<Event> disruptor(16);
    auto* ring_buffer = disruptor.getRingBuffer();
    auto* cursor = ring_buffer->getCursor();

    std::cout << "Initial cursor position: " << cursor->get() << "\n";

    for (int i = 0; i < 5; i++) {
        int64_t seq = i;
        Event& event = ring_buffer->get(seq);
        event.value = i * 100;
        event.timestamp = i;
        ring_buffer->publish(seq);
        std::cout << "Produced event " << i << " at sequence " << seq << "\n";
    }

    std::cout << "Final cursor position: " << cursor->get() << "\n\n";

    for (int64_t i = 0; i <= cursor->get(); i++) {
        const Event& event = ring_buffer->get(i);
        std::cout << "Seq " << i << ": value=" << event.value
                  << ", timestamp=" << event.timestamp << "\n";
    }
}

} // namespace

int main() {
    std::cout << "\n";
    std::cout << "==============================================\n";
    std::cout << "  LMAX Disruptor - High-Performance Ring Buffer\n";
    std::cout << "==============================================\n";

    std::cout << "\nKey Components:\n";
    std::cout << "  - Sequence (cache-line aligned)\n";
    std::cout << "  - RingBuffer (pre-allocated)\n";
    std::cout << "  - ClaimStrategy (single producer)\n";
    std::cout << "  - WaitStrategy (busy spin / yielding)\n";
    std::cout << "  - ProducerBarrier / ConsumerBarrier\n";
    std::cout << "  - BatchHandler / Consumer\n";

    runSimpleRingBufferDemo();

    std::cout << "\nNote: Full benchmark and pipeline demos are enabled.\n";
    std::cout << "They use a single producer, which is the supported mode here.\n";

    runBenchmark();
    runPipelineDemo();

    return 0;
}
