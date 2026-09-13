#include <cstdint>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Value.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

static_assert(type::from<long>() != TYPE_OBJECT);
static_assert(type::from<unsigned long>() != TYPE_OBJECT);
static_assert(type::from<long long>() != TYPE_OBJECT);
static_assert(type::from<unsigned long long>() != TYPE_OBJECT);
static_assert(type::from<long double>() == TYPE_OBJECT);

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

    long long testll = 64;
    val = testll;
    EXPECT_TRUE(val == testll);
    unsigned long long testull = 64;
    val = testull;
    EXPECT_TRUE(val == testull);
    unsigned long testul = 64;
    val = testul;
    EXPECT_TRUE(val == testul);
}

TEST_F(TestValue, test_value_str_types)
{
    Value val(true);
    EXPECT_EQ(val.type, TYPE_BOOL);
    EXPECT_EQ(val.str(), "true");
    val = false;
    EXPECT_EQ(val.str(), "false");

    const uint64_t uint64_max = std::numeric_limits<uint64_t>::max();
    val = uint64_max;
    EXPECT_EQ(val.type, TYPE_ULONG);
    EXPECT_EQ(val.str(), "18446744073709551615");

    val = Value::from_int_string("u18446744073709551615");
    EXPECT_EQ(val.type, TYPE_ULONG);
    EXPECT_EQ(val.str(), "18446744073709551615");
}

TEST_F(TestValue, test_value_compare_unsigned)
{
    const uint64_t high_bit = 1ull << 63;
    const uint64_t uint64_max = std::numeric_limits<uint64_t>::max();

    Value vhigh(high_bit);
    Value vmax(uint64_max);
    Value vminus_one(-1);

    EXPECT_TRUE(vhigh == high_bit);
    EXPECT_TRUE(vmax == uint64_max);
    EXPECT_TRUE(vmax > high_bit);
    EXPECT_TRUE(vhigh > 0);
    EXPECT_TRUE(vhigh >= 0);

    EXPECT_TRUE(vmax > Value((int64_t)5));
    EXPECT_TRUE(Value((int64_t)5) < vmax);
    EXPECT_TRUE(vmax > std::numeric_limits<int64_t>::max());
    EXPECT_TRUE(Value(std::numeric_limits<int64_t>::max()) < vmax);

    EXPECT_TRUE(vminus_one < vmax);
    EXPECT_TRUE(vmax != vminus_one);
    EXPECT_FALSE(vminus_one == vmax);
    EXPECT_TRUE(vminus_one < (uint64_t)0);
    EXPECT_TRUE(Value(-2) < (uint64_t)0);

    const uint32_t uint32_max = std::numeric_limits<uint32_t>::max();
    EXPECT_TRUE(Value(uint32_max) == uint32_max);
    EXPECT_TRUE(Value(uint32_max) != (int32_t)-1);
    EXPECT_TRUE(Value(-1) != uint32_max);
    EXPECT_TRUE(Value(uint32_max) < high_bit);
}

TEST_F(TestValue, test_value_from_buffer)
{
    const uint64_t raw64 = 0x1122334455667788ull;
    Value val((const uint8_t *)&raw64, TYPE_ULONG);
    EXPECT_EQ(val.type, TYPE_ULONG);
    EXPECT_TRUE(val == raw64);

    const uint8_t raw8 = 200;
    Value vbyte((const uint8_t *)&raw8, TYPE_UBYTE);
    EXPECT_EQ(vbyte.data.un, 200ull);
    EXPECT_TRUE(vbyte == 200);

    const float rawf = 1.5f;
    Value vfloat((const uint8_t *)&rawf, TYPE_FLOAT);
    EXPECT_EQ(vfloat.type, TYPE_FLOAT);
    EXPECT_TRUE(vfloat == rawf);

    EXPECT_THROW(Value((const uint8_t *)&raw64, TYPE_NONE), std::invalid_argument);
    EXPECT_THROW(Value((const uint8_t *)&raw64, TYPE_OBJECT), std::invalid_argument);
}

TEST_F(TestValue, test_value_compare_mixed_float)
{
    Value v5(5);
    Value vf(5.5f);

    EXPECT_TRUE(v5 < 5.5);
    EXPECT_TRUE(v5 > 4.5);
    EXPECT_TRUE(v5.compare(5.5) < 0);
    EXPECT_TRUE(v5.compare(5.0) == 0);
    EXPECT_TRUE(v5.compare(4.9) > 0);
    EXPECT_TRUE(v5 < 5.5f);
    EXPECT_TRUE(v5 > 4.9f);

    EXPECT_TRUE(vf > 5);
    EXPECT_TRUE(vf < 6);
    EXPECT_TRUE(vf > Value(5));
    EXPECT_TRUE(Value(5) < vf);
    EXPECT_TRUE(vf > Value(5.0));
    EXPECT_TRUE(Value(5.0) < vf);
}

} // namespace test
