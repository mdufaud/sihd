#include <gtest/gtest.h>

#include <sihd/util/num.hpp>

namespace test
{
using namespace sihd::util;
class TestNum: public ::testing::Test
{
    protected:
        TestNum() {}

        virtual ~TestNum() {}

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestNum, test_num)
{
    constexpr uint16_t base2 = 2;
    constexpr uint16_t base10 = 10;
    constexpr uint16_t base16 = 16;

    EXPECT_EQ(num::size(0b10001, base2), 5u);
    EXPECT_EQ(num::size(1000, base10), 4u);
    EXPECT_EQ(num::size(0xFF, base16), 2u);

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
} // namespace test
