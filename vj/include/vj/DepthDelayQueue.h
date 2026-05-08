#pragma once

#include <cstddef>
#include <deque>
#include <functional>

#include "Primitive.h"

namespace vj {

class DepthDelayQueue {
public:
    using SubmitFn = std::function<void(const Primitive&)>;

    explicit DepthDelayQueue(std::size_t capacity = 64);

    void push(const Primitive& prim, int delaySlots, const SubmitFn& submit);
    void tickAndFlush(const SubmitFn& submit);
    void flushAll(const SubmitFn& submit);

    std::size_t size() const { return m_queue.size(); }
    std::size_t capacity() const { return m_capacity; }

private:
    struct Entry {
        Primitive prim;
        int slots;
    };
    std::deque<Entry> m_queue;
    std::size_t m_capacity;
};

}
