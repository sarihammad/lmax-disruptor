#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "disruptor/sequence.h"

namespace disruptor {

template <typename T>
struct DefaultEntryFactory {
    void construct(T* ptr) const { new (ptr) T(); }
    void destroy(T* ptr) const { ptr->~T(); }
    void reset(T&) const {}
};

template <typename T, typename EntryFactory = DefaultEntryFactory<T>>
class RingBuffer {
public:
    explicit RingBuffer(size_t size, EntryFactory entry_factory = EntryFactory())
        : buffer_size_(roundUpToPowerOfTwo(size))
        , index_mask_(buffer_size_ - 1)
        , entries_(buffer_size_)
        , entry_factory_(std::move(entry_factory)) {
        static_assert(std::is_nothrow_destructible_v<T>,
                      "RingBuffer entries must be nothrow destructible");
        for (size_t i = 0; i < buffer_size_; ++i) {
            entry_factory_.construct(entryPointer(i));
        }
    }

    ~RingBuffer() {
        for (size_t i = 0; i < buffer_size_; ++i) {
            entry_factory_.destroy(entryPointer(i));
        }
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    size_t getBufferSize() const { return buffer_size_; }

    T& get(int64_t sequence) {
        return *entryPointer(sequence & index_mask_);
    }

    const T& get(int64_t sequence) const {
        return *entryPointer(sequence & index_mask_);
    }

    void prepareForWrite(int64_t sequence) {
        entry_factory_.reset(get(sequence));
    }

    Sequence* getCursor() { return &cursor_; }
    const Sequence* getCursor() const { return &cursor_; }

    void publish(int64_t sequence) {
        cursor_.setMonotonic(sequence);
    }

    void publish(int64_t /*lo*/, int64_t hi) {
        cursor_.setMonotonic(hi);
    }

private:
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    T* entryPointer(size_t index) {
        return std::launder(reinterpret_cast<T*>(&entries_[index]));
    }

    const T* entryPointer(size_t index) const {
        return std::launder(reinterpret_cast<const T*>(&entries_[index]));
    }

    static size_t roundUpToPowerOfTwo(size_t v) {
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        v++;
        return v;
    }

    const size_t buffer_size_;
    const size_t index_mask_;
    std::vector<Storage> entries_;
    Sequence cursor_{-1};
    EntryFactory entry_factory_;
};

} // namespace disruptor
