#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <vector>

#include "vj/DepthDelayQueue.h"
#include "vj/Primitive.h"

namespace {

vj::Primitive makePrim(int marker) {
    vj::Primitive p;
    p.kind = vj::PrimitiveKind::Triangle;
    vj::Vertex v;
    v.x = static_cast<float>(marker);
    p.vertices.push_back(v);
    p.vertices.push_back(vj::Vertex{});
    p.vertices.push_back(vj::Vertex{});
    return p;
}

int markerOf(const vj::Primitive& p) { return static_cast<int>(p.vertices[0].x); }

void test_zero_delay_submits_immediately() {
    vj::DepthDelayQueue q;
    std::vector<int> out;
    auto submit = [&](const vj::Primitive& p) { out.push_back(markerOf(p)); };

    q.push(makePrim(7), 0, submit);
    assert(out.size() == 1);
    assert(out[0] == 7);
    assert(q.size() == 0);
}

void test_delayed_releases_after_ticks() {
    vj::DepthDelayQueue q;
    std::vector<int> out;
    auto submit = [&](const vj::Primitive& p) { out.push_back(markerOf(p)); };

    q.push(makePrim(1), 3, submit);
    assert(q.size() == 1);
    assert(out.empty());

    q.tickAndFlush(submit);
    assert(out.empty());
    q.tickAndFlush(submit);
    assert(out.empty());
    q.tickAndFlush(submit);
    assert(out.size() == 1);
    assert(out[0] == 1);
    assert(q.size() == 0);
}

void test_mixed_delays_release_in_order() {
    vj::DepthDelayQueue q;
    std::vector<int> out;
    auto submit = [&](const vj::Primitive& p) { out.push_back(markerOf(p)); };

    q.push(makePrim(10), 1, submit);
    q.push(makePrim(20), 2, submit);
    q.push(makePrim(30), 3, submit);
    assert(q.size() == 3);

    q.tickAndFlush(submit);  // 10 fires
    assert(out.size() == 1 && out[0] == 10);
    assert(q.size() == 2);

    q.tickAndFlush(submit);  // 20 fires
    assert(out.size() == 2 && out[1] == 20);

    q.tickAndFlush(submit);  // 30 fires
    assert(out.size() == 3 && out[2] == 30);
    assert(q.size() == 0);
}

void test_capacity_overflow_falls_back_to_immediate() {
    vj::DepthDelayQueue q(2);
    std::vector<int> out;
    auto submit = [&](const vj::Primitive& p) { out.push_back(markerOf(p)); };

    q.push(makePrim(1), 5, submit);
    q.push(makePrim(2), 5, submit);
    assert(q.size() == 2);
    assert(out.empty());

    q.push(makePrim(3), 5, submit);
    assert(q.size() == 2);
    assert(out.size() == 1 && out[0] == 3);
}

void test_flush_all_drains_queue() {
    vj::DepthDelayQueue q;
    std::vector<int> out;
    auto submit = [&](const vj::Primitive& p) { out.push_back(markerOf(p)); };

    q.push(makePrim(100), 10, submit);
    q.push(makePrim(200), 20, submit);
    q.flushAll(submit);
    assert(q.size() == 0);
    assert(out.size() == 2);
    assert(out[0] == 100 && out[1] == 200);
}

}  // namespace

int main() {
    test_zero_delay_submits_immediately();
    test_delayed_releases_after_ticks();
    test_mixed_delays_release_in_order();
    test_capacity_overflow_falls_back_to_immediate();
    test_flush_all_drains_queue();
    std::fprintf(stderr, "[test_depth_delay_queue] OK\n");
    return 0;
}
