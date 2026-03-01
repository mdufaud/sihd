#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Value.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::util;
class TestValue: public ::testing::Test
{
    protected:
        TestValue() { sihd::util::LoggerManager::stream(); }

        virtual ~TestValue() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestValue, test_value_str)
{
    Value val;

    val = Value::from_any_string("0x200");
    EXPECT_FALSE(val.empty());
    EXPECT_EQ(val.str(), "512");

    val = Value::from_any_string("0b10");
    EXPECT_FALSE(val.empty());
    EXPECT_EQ(val.str(), "2");

    val = Value::from_any_string("-1");
    EXPECT_FALSE(val.empty());
    EXPECT_EQ(val.str(), "-1");
}

TEST_F(TestValue, test_value_compare)
{
    Value v5(5);
    Value v10(10);
    Value v10_same = 10;
    Value v11_float = 11.2f;
    Value v11_float_same(11.2f);

    EXPECT_TRUE(v10 > v5);
    EXPECT_TRUE(v10 != v5);

    EXPECT_TRUE(v10 == v10_same);
    EXPECT_TRUE(v10_same == v10);
    EXPECT_TRUE(v10 >= v10_same);
    EXPECT_TRUE(v10 <= v10_same);
    EXPECT_FALSE(v10 != v10_same);

    EXPECT_TRUE(v10 != v11_float);
    EXPECT_TRUE(v11_float != v10);
    EXPECT_TRUE(v10 < v11_float);
    EXPECT_TRUE(v11_float > v10);

    EXPECT_TRUE(v11_float_same == v11_float);
    EXPECT_TRUE(v11_float >= v11_float_same);
    EXPECT_TRUE(v11_float <= v11_float_same);
    EXPECT_FALSE(v11_float != v11_float_same);
}

TEST_F(TestValue, test_value_float)
{
    Value val(3.124f);

    EXPECT_TRUE(val > 3.123f);
    EXPECT_TRUE(val > 3.123);
    EXPECT_TRUE(val == 3.124f);
    EXPECT_TRUE(val == 3.124);
    EXPECT_TRUE(val < 3.125f);
    EXPECT_TRUE(val < 3.125);

    EXPECT_TRUE(val < 4);
    EXPECT_TRUE(val > 3);

    // double
    val = 6.28;

    EXPECT_TRUE(val > 6.27);
    EXPECT_TRUE(val > 6.27f);
    EXPECT_TRUE(val == 6.28);
    // comparing to float requires higher epsilon than machine's
    EXPECT_TRUE(val.compare_float_epsilon(6.28f, 0.00001) == 0);
    EXPECT_TRUE(val < 6.29);
    EXPECT_TRUE(val < 6.29f);

    EXPECT_TRUE(val < 7);
    EXPECT_TRUE(val > 6);
}

TEST_F(TestValue, test_value_max)
{
    uint8_t test8 = 255;
    Value val(test8);

    EXPECT_TRUE(val < 2000);
    EXPECT_TRUE(val == 255);
    EXPECT_TRUE(val > 254);
}

TEST_F(TestValue, test_value)
{
    int8_t test8 = 8;
    int16_t test16 = 16;
    int32_t test32 = 32;
    int64_t test64 = 64;

    Value val;
    val = true;
    EXPECT_TRUE(val == true);
    EXPECT_TRUE(val != false);
    val = 'c';
    EXPECT_TRUE(val == 'c');
    EXPECT_TRUE(val > 'b');
    EXPECT_TRUE(val < 'd');
    val = test8;
    EXPECT_TRUE(val == test8);
    EXPECT_TRUE(val < test16);
    EXPECT_TRUE(val <= test16);
    EXPECT_FALSE(val > test16);
    EXPECT_FALSE(val >= test16);
    val = test16;
    EXPECT_TRUE(val == test16);
    val = test32;
    EXPECT_TRUE(val == test32);
    val = test64;
    EXPECT_TRUE(val == test64);
}

} // namespace test
