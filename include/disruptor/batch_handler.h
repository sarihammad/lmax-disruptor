#pragma once

#include <cstdint>

namespace disruptor {

template <typename T>
class BatchHandler {
public:
    virtual ~BatchHandler() = default;

    virtual void onAvailable(const T& entry, int64_t sequence, bool end_of_batch) = 0;

    virtual void onCompletion() {}
};

} // namespace disruptor
