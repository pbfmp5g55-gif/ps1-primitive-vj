#include "vj/DepthDelayQueue.h"

namespace vj {

DepthDelayQueue::DepthDelayQueue(std::size_t capacity) : m_capacity(capacity) {}

void DepthDelayQueue::push(const Primitive& prim, int delaySlots, const SubmitFn& submit) {
    if (delaySlots < 1) {
        submit(prim);
        return;
    }
    if (m_queue.size() >= m_capacity) {
        submit(prim);
        return;
    }
    m_queue.push_back(Entry{prim, delaySlots});
}

void DepthDelayQueue::tickAndFlush(const SubmitFn& submit) {
    std::deque<Entry> kept;
    for (auto& e : m_queue) {
        e.slots--;
        if (e.slots <= 0) {
            submit(e.prim);
        } else {
            kept.push_back(std::move(e));
        }
    }
    m_queue = std::move(kept);
}

void DepthDelayQueue::flushAll(const SubmitFn& submit) {
    for (auto& e : m_queue) submit(e.prim);
    m_queue.clear();
}

}
