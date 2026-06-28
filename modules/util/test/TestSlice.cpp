#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Slice.hpp>

namespace test
{
SIHD_LOGGER;

using namespace sihd::util;

class TestSlice: public ::testing::Test
{
    protected:
        TestSlice() { sihd::util::LoggerManager::stream(); }

        virtual ~TestSlice() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestSlice, test_resolve_defaults)
{
    constexpr size_t container_size = 10;
    Slice s;
    auto range = s.resolve(container_size);
    EXPECT_EQ(range.from, 0u);
    EXPECT_EQ(range.to, 10u);
    EXPECT_EQ(range.size(), 10u);
    EXPECT_FALSE(range.empty());
}

TEST_F(TestSlice, test_resolve_positive)
{
    constexpr size_t container_size = 10;
    auto range = Slice {2, 5}.resolve(container_size);
    EXPECT_EQ(range.from, 2u);
    EXPECT_EQ(range.to, 6u);
    EXPECT_EQ(range.size(), 4u);
}

TEST_F(TestSlice, test_resolve_from_only)
{
    constexpr size_t container_size = 10;
    auto range = Slice {3}.resolve(container_size);
    EXPECT_EQ(range.from, 3u);
    EXPECT_EQ(range.to, 10u);
    EXPECT_EQ(range.size(), 7u);
    EXPECT_FALSE(range.empty());
}

TEST_F(TestSlice, test_resolve_negative_from)
{
    constexpr size_t container_size = 10;
    auto range = Slice {-3, -1}.resolve(container_size);
    EXPECT_EQ(range.from, 7u);
    EXPECT_EQ(range.to, 10u);
    EXPECT_EQ(range.size(), 3u);
}

TEST_F(TestSlice, test_resolve_negative_both)
{
    constexpr size_t container_size = 10;
    auto range = Slice {-30, -2}.resolve(container_size);
    EXPECT_EQ(range.from, 0u);
    EXPECT_EQ(range.to, 9u);
    EXPECT_EQ(range.size(), 9u);
}

TEST_F(TestSlice, test_resolve_clamp_out_of_range)
{
    constexpr size_t container_size = 5;
    auto range = Slice {0, 100}.resolve(container_size);
    EXPECT_EQ(range.from, 0u);
    EXPECT_EQ(range.to, 5u);
    EXPECT_EQ(range.size(), 5u);
}

TEST_F(TestSlice, test_resolve_empty_range)
{
    constexpr size_t container_size = 10;
    auto range = Slice {5, 2}.resolve(container_size);
    EXPECT_TRUE(range.empty());
    EXPECT_EQ(range.size(), 0u);
}

TEST_F(TestSlice, test_resolve_empty_size)
{
    constexpr size_t container_size = 0;
    auto range = Slice {}.resolve(container_size);
    EXPECT_TRUE(range.empty());
}

TEST_F(TestSlice, test_resolve_single_element)
{
    constexpr size_t container_size = 10;
    auto range = Slice {3, 3}.resolve(container_size);
    EXPECT_EQ(range.from, 3u);
    EXPECT_EQ(range.to, 4u);
    EXPECT_EQ(range.size(), 1u);
}

TEST_F(TestSlice, test_resolve_last_element)
{
    constexpr size_t container_size = 10;
    auto range = Slice {-1, -1}.resolve(container_size);
    EXPECT_EQ(range.from, 9u);
    EXPECT_EQ(range.to, 10u);
    EXPECT_EQ(range.size(), 1u);
}

TEST_F(TestSlice, test_resolve_negative_overflow)
{
    constexpr size_t container_size = 5;
    auto range = Slice {-100, -1}.resolve(container_size);
    EXPECT_EQ(range.from, 0u);
    EXPECT_EQ(range.to, 5u);
    EXPECT_EQ(range.size(), 5u);
}

TEST_F(TestSlice, test_from_size_basic)
{
    auto s = Slice::from_size(2, 4);
    auto range = s.resolve(10);
    EXPECT_EQ(range.from, 2u);
    EXPECT_EQ(range.to, 6u);
    EXPECT_EQ(range.size(), 4u);
}

TEST_F(TestSlice, test_from_size_zero_offset)
{
    auto s = Slice::from_size(0, 3);
    auto range = s.resolve(10);
    EXPECT_EQ(range.from, 0u);
    EXPECT_EQ(range.to, 3u);
    EXPECT_EQ(range.size(), 3u);
}

TEST_F(TestSlice, test_from_size_single)
{
    auto s = Slice::from_size(5, 1);
    auto range = s.resolve(10);
    EXPECT_EQ(range.from, 5u);
    EXPECT_EQ(range.to, 6u);
    EXPECT_EQ(range.size(), 1u);
}

TEST_F(TestSlice, test_from_size_clamp)
{
    auto s = Slice::from_size(8, 10);
    auto range = s.resolve(10);
    EXPECT_EQ(range.from, 8u);
    EXPECT_EQ(range.to, 10u);
    EXPECT_EQ(range.size(), 2u);
}

TEST_F(TestSlice, test_from_size_out_of_range)
{
    auto s = Slice::from_size(20, 5);
    auto range = s.resolve(10);
    EXPECT_TRUE(range.empty());
}

} // namespace test
