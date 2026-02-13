#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "disruptor/sequence.h"

namespace disruptor {

inline int64_t getMinimumSequence(Sequence* cursor,
                                  const std::vector<Sequence*>& dependents) {
    int64_t minimum = cursor ? cursor->get() : std::numeric_limits<int64_t>::max();
    for (auto* seq : dependents) {
        int64_t value = seq->get();
        if (value < minimum) {
            minimum = value;
        }
    }
    return minimum;
}

} // namespace disruptor
