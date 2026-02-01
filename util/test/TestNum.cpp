#include <gtest/gtest.h>

#include <sihd/util/num.hpp>

namespace test
{
using namespace sihd::util;
class TestNum: public ::testing::Test
{
    protected:
        TestNum() = default;

        virtual ~TestNum() = default;

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestNum, test_num_size)
{
    constexpr uint16_t base2 = 2;
    constexpr uint16_t base10 = 10;
    constexpr uint16_t base16 = 16;

    EXPECT_EQ(num::size(0b10001, base2), 5u);
    EXPECT_EQ(num::size(0, base10), 1u);
    EXPECT_EQ(num::size(1, base10), 1u);
    EXPECT_EQ(num::size(1000, base10), 4u);
    EXPECT_EQ(num::size(0xFF, base16), 2u);
}

TEST_F(TestNum, test_num_near)
{
    EXPECT_TRUE(num::near(5, 5, 0));
    EXPECT_TRUE(num::near(5, 5, 1));
    EXPECT_TRUE(num::near(5, 5, 2));

    EXPECT_FALSE(num::near(5, 3, 1));

    EXPECT_TRUE(num::near(10, 20, 10));
    EXPECT_TRUE(num::near(-10, 20, 30));
    EXPECT_TRUE(num::near(10, -20, 30));

    EXPECT_FALSE(num::near(10, 20, 9));
    EXPECT_FALSE(num::near(-10, 20, 29));
    EXPECT_FALSE(num::near(10, -20, 29));
}

TEST_F(TestNum, test_num_add_no_overflow)
{
    EXPECT_EQ(num::add_no_overflow(10, 20), 30);
    EXPECT_EQ(num::add_no_overflow(10, 0), 10);
    EXPECT_EQ(num::add_no_overflow(0, 20), 20);
    EXPECT_EQ(num::add_no_overflow(0, 0), 0);

    EXPECT_EQ(num::add_no_overflow(std::numeric_limits<uint64_t>::max(), 1ul),
              std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(num::add_no_overflow(std::numeric_limits<int64_t>::max(), 1l),
              std::numeric_limits<int64_t>::max());

    EXPECT_EQ(num::add_no_overflow(std::numeric_limits<uint64_t>::max(), 0ul),
              std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(num::add_no_overflow(std::numeric_limits<int64_t>::min(), -1l),
              std::numeric_limits<int64_t>::min());
}

TEST_F(TestNum, test_num_substract_no_overflow)
{
    EXPECT_EQ(num::substract_no_overflow(30, 20), 10);
    EXPECT_EQ(num::substract_no_overflow(10, 0), 10);
    EXPECT_EQ(num::substract_no_overflow(0, 20), -20);
    EXPECT_EQ(num::substract_no_overflow(0, 0), 0);

    EXPECT_EQ(num::substract_no_overflow(std::numeric_limits<uint64_t>::max(), 1ul),
              std::numeric_limits<uint64_t>::max() - 1);
    EXPECT_EQ(num::substract_no_overflow(std::numeric_limits<int64_t>::min(), -1l),
              std::numeric_limits<int64_t>::min() + 1);

    EXPECT_EQ(num::substract_no_overflow(std::numeric_limits<uint64_t>::max(),
                                         std::numeric_limits<uint64_t>::max()),
              0u);
    EXPECT_EQ(
        num::substract_no_overflow(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::min()),
        0l);
}

} // namespace test
