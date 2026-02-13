# LMAX Disruptor

An implementation of the LMAX Disruptor pattern.

## Architecture

```mermaid
flowchart LR
    Producer -->|claim next| Claim[ClaimStrategy]
    Claim -->|sequence| Producer
    Producer -->|write| RB[RingBuffer]
    Producer -->|publish| Cursor[Cursor Sequence]

    subgraph Consumers
        Cursor --> Barrier[ConsumerBarrier]
        Deps[Dependency Sequences] --> Barrier
        Barrier --> Handler[BatchHandler]
    end
```

## Component Map

```mermaid
flowchart TB
    Disruptor --> RingBuffer
    Disruptor --> ProducerBarrier
    Disruptor --> ConsumerBarrier
    Disruptor --> ClaimStrategy
    Disruptor --> WaitStrategy
    ConsumerBarrier --> WaitStrategy
    ProducerBarrier --> ClaimStrategy
    ProducerBarrier --> RingBuffer
    ConsumerBarrier --> RingBuffer
```

## Build & Run

```bash
g++ -std=c++17 -O3 -pthread -Iinclude examples/demo.cpp -o disruptor
./disruptor
```
