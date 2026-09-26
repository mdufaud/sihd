#include <span>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/RingBuffer.hpp>

namespace test
{
using namespace sihd::util;

TEST(TestRingBuffer, test_ring_buffer_write_read)
{
    RingBuffer<int> ring(4);
    EXPECT_EQ(ring.capacity(), 4u);
    EXPECT_TRUE(ring.empty());
    EXPECT_EQ(ring.available(), 4u);

    EXPECT_EQ(ring.write(std::vector<int> {1, 2, 3}), 3u);
    EXPECT_EQ(ring.size(), 3u);
    EXPECT_FALSE(ring.empty());
    EXPECT_FALSE(ring.full());

    int out[4] = {};
    EXPECT_EQ(ring.peek(out, 4), 3u);
    EXPECT_EQ(out[0], 1);
    EXPECT_EQ(out[2], 3);
    EXPECT_EQ(ring.size(), 3u); // peek does not advance

    EXPECT_EQ(ring.read(out, 2), 2u);
    EXPECT_EQ(out[0], 1);
    EXPECT_EQ(out[1], 2);
    EXPECT_EQ(ring.size(), 1u);

    EXPECT_EQ(ring.skip(10), 1u);
    EXPECT_TRUE(ring.empty());
}

TEST(TestRingBuffer, test_ring_buffer_full)
{
    RingBuffer<int> ring(2);
    EXPECT_EQ(ring.write(std::vector<int> {1, 2}), 2u);
    EXPECT_TRUE(ring.full());
    EXPECT_EQ(ring.available(), 0u);
    // bounded: refuses rather than overwrites
    EXPECT_EQ(ring.write(std::vector<int> {3}), 0u);
    // partial writes clamp to the available space
    EXPECT_EQ(ring.skip(1), 1u);
    EXPECT_EQ(ring.write(std::vector<int> {3, 4, 5}), 1u);
    EXPECT_TRUE(ring.full());

    int out[2] = {};
    EXPECT_EQ(ring.read(out, 2), 2u);
    EXPECT_EQ(out[0], 2);
    EXPECT_EQ(out[1], 3);
    EXPECT_TRUE(ring.empty());
}

TEST(TestRingBuffer, test_ring_buffer_wrap)
{
    RingBuffer<int> ring(4);
    EXPECT_EQ(ring.write(std::vector<int> {1, 2, 3, 4}), 4u);

    int out[2] = {};
    EXPECT_EQ(ring.read(out, 2), 2u);
    // wraps over the consumed head
    EXPECT_EQ(ring.write(std::vector<int> {5, 6}), 2u);
    EXPECT_EQ(ring.size(), 4u);

    EXPECT_EQ(ring.read(out, 2), 2u);
    EXPECT_EQ(out[0], 3);
    EXPECT_EQ(out[1], 4);
    EXPECT_EQ(ring.read(out, 2), 2u);
    EXPECT_EQ(out[0], 5);
    EXPECT_EQ(out[1], 6);
    EXPECT_TRUE(ring.empty());
}

TEST(TestRingBuffer, test_ring_buffer_split)
{
    RingBuffer<int> ring(4);
    EXPECT_EQ(ring.write(std::vector<int> {1, 2, 3}), 3u);

    int out = 0;
    EXPECT_EQ(ring.read(&out, 1), 1u);
    // 4 wraps to index 0, 5 lands at index 1
    EXPECT_EQ(ring.write(std::vector<int> {4, 5}), 2u);
    EXPECT_EQ(ring.size(), 4u);

    int all[4] = {};
    EXPECT_EQ(ring.read(all, 4), 4u);
    EXPECT_EQ(all[0], 2);
    EXPECT_EQ(all[1], 3);
    EXPECT_EQ(all[2], 4);
    EXPECT_EQ(all[3], 5);
}

TEST(TestRingBuffer, test_ring_buffer_read_span)
{
    RingBuffer<int> ring(4);
    EXPECT_EQ(ring.write(std::vector<int> {1, 2, 3, 4}), 4u);

    ArrayView<int> span = ring.read_span();
    EXPECT_EQ(span.size(), 4u);
    EXPECT_EQ(span[0], 1);
    EXPECT_EQ(span[3], 4);

    int out = 0;
    ASSERT_EQ(ring.read(&out, 1), 1u);
    EXPECT_EQ(ring.write(std::vector<int> {5}), 1u);

    // at most one contiguous chunk: {2, 3, 4} then {5}
    span = ring.read_span();
    EXPECT_EQ(span.size(), 3u);
    EXPECT_EQ(span[0], 2);
    EXPECT_EQ(ring.skip(span.size()), 3u);
    span = ring.read_span();
    EXPECT_EQ(span.size(), 1u);
    EXPECT_EQ(span[0], 5);
}

TEST(TestRingBuffer, test_ring_buffer_write_span)
{
    RingBuffer<int> ring(4);
    std::span<int> span = ring.write_span();
    EXPECT_EQ(span.size(), 4u);
    span[0] = 99;
    EXPECT_EQ(ring.size(), 0u); // nothing counted until produce

    EXPECT_EQ(ring.write(std::vector<int> {1, 2, 3}), 3u);
    EXPECT_EQ(ring.produce(1), 0u);
    EXPECT_EQ(ring.skip(3), 3u); // head wraps to 3, ring empty

    // single free slot at the physical end
    span = ring.write_span();
    EXPECT_EQ(span.size(), 1u);
    span[0] = 4;
    EXPECT_EQ(ring.produce(2), 1u);
    EXPECT_EQ(ring.produce(1), 0u);

    // remaining free space is contiguous at the physical start
    span = ring.write_span();
    EXPECT_EQ(span.size(), 3u);
    for (size_t i = 0; i < span.size(); ++i)
        span[i] = int(i) + 5;
    EXPECT_EQ(ring.produce(3), 3u);
    EXPECT_TRUE(ring.full());

    int out[4] = {};
    EXPECT_EQ(ring.read(out, 4), 4u);
    EXPECT_EQ(out[0], 4);
    EXPECT_EQ(out[1], 5);
    EXPECT_EQ(out[2], 6);
    EXPECT_EQ(out[3], 7);
}

TEST(TestRingBuffer, test_ring_buffer_zero_capacity)
{
    RingBuffer<int> ring(0);
    EXPECT_EQ(ring.capacity(), 0u);
    EXPECT_TRUE(ring.empty());
    EXPECT_EQ(ring.write(std::vector<int> {1}), 0u);
    int out = 0;
    EXPECT_EQ(ring.read(&out, 1), 0u);
    EXPECT_EQ(ring.skip(1), 0u);
    EXPECT_EQ(ring.read_span().size(), 0u);
    EXPECT_EQ(ring.write_span().size(), 0u);
    EXPECT_EQ(ring.produce(1), 0u);
    ring.clear();
    EXPECT_TRUE(ring.empty());
}

TEST(TestRingBuffer, test_ring_buffer_clear)
{
    RingBuffer<int> ring(4);
    EXPECT_EQ(ring.write(std::vector<int> {1, 2}), 2u);
    ring.clear();
    EXPECT_TRUE(ring.empty());
    EXPECT_EQ(ring.available(), 4u);
    EXPECT_EQ(ring.write(std::vector<int> {3, 4}), 2u);

    int out[2] = {};
    EXPECT_EQ(ring.read(out, 2), 2u);
    EXPECT_EQ(out[0], 3);
    EXPECT_EQ(out[1], 4);
}

} // namespace test
