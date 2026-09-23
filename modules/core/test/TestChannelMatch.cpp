#include <gtest/gtest.h>

#include <sihd/core/Channel.hpp>
#include <sihd/core/ChannelMatch.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::core;
using namespace sihd::util;

class TestChannelMatch: public ::testing::Test
{
    protected:
        TestChannelMatch() { sihd::util::LoggerManager::stream(); }

        virtual ~TestChannelMatch() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestChannelMatch, test_parse_trigger)
{
    ChannelMatch match;
    EXPECT_TRUE(match.parse_trigger("15"));
    EXPECT_EQ(match.idx, 0u);
    EXPECT_TRUE(match.value == 15);

    EXPECT_TRUE(match.parse_trigger("2:0x10"));
    EXPECT_EQ(match.idx, 2u);
    EXPECT_TRUE(match.value == 16);

    EXPECT_TRUE(match.parse_trigger("false"));
    EXPECT_TRUE(match.value == Value(false));

    EXPECT_TRUE(match.parse_trigger("1:"));
    EXPECT_EQ(match.idx, 1u);

    EXPECT_FALSE(match.parse_trigger(""));
    EXPECT_FALSE(match.parse_trigger(":"));
    EXPECT_FALSE(match.parse_trigger("1:2:3"));
    EXPECT_FALSE(match.parse_trigger("nope"));
    EXPECT_FALSE(match.parse_trigger("x:5"));
}

TEST_F(TestChannelMatch, test_parse)
{
    ChannelMatch match;
    EXPECT_TRUE(match.parse("cmp=superior;idx=3;value=10"));
    EXPECT_EQ(match.comparison, ChannelMatch::Superior);
    EXPECT_EQ(match.idx, 3u);
    EXPECT_TRUE(match.value == 10);
    EXPECT_FALSE(match.invert);

    EXPECT_TRUE(match.parse("cmp=equal;value=false;invert=true"));
    EXPECT_EQ(match.comparison, ChannelMatch::Equal);
    EXPECT_TRUE(match.value == Value(false));
    EXPECT_TRUE(match.invert);

    EXPECT_STREQ(ChannelMatch::comparison_str(ChannelMatch::ByteAnd), "byte_and");
    EXPECT_EQ(ChannelMatch::comparison_from_str("byte_xor"), ChannelMatch::ByteXor);
    EXPECT_EQ(ChannelMatch::comparison_from_str("nope"), ChannelMatch::None);

    EXPECT_FALSE(match.parse("value=1"));
    EXPECT_FALSE(match.parse("cmp=nope"));
    EXPECT_FALSE(match.parse("cmp=equal;idx=aa"));
    EXPECT_FALSE(match.parse("cmp=equal;value=nope"));
    EXPECT_FALSE(match.parse("cmp=equal;invert=maybe"));
}

TEST_F(TestChannelMatch, test_match)
{
    Channel chan("chan", "int", 3);
    ASSERT_TRUE(chan.write<int>(0, 10));
    ASSERT_TRUE(chan.write<int>(1, 20));
    ASSERT_TRUE(chan.write<int>(2, 30));

    EXPECT_TRUE(ChannelMatch(ChannelMatch::Equal, Value(20), 1).match(&chan));
    EXPECT_FALSE(ChannelMatch(ChannelMatch::Equal, Value(20), 0).match(&chan));
    EXPECT_TRUE(ChannelMatch(ChannelMatch::Superior, Value(15), 1).match(&chan));
    EXPECT_TRUE(ChannelMatch(ChannelMatch::SuperiorEqual, Value(30), 2).match(&chan));
    EXPECT_TRUE(ChannelMatch(ChannelMatch::Inferior, Value(15), 0).match(&chan));
    EXPECT_TRUE(ChannelMatch(ChannelMatch::InferiorEqual, Value(10), 0).match(&chan));

    EXPECT_TRUE(ChannelMatch(ChannelMatch::ByteAnd, Value(0b10100), 1).match(&chan));
    EXPECT_FALSE(ChannelMatch(ChannelMatch::ByteAnd, Value(0b1), 1).match(&chan));
    EXPECT_TRUE(ChannelMatch(ChannelMatch::ByteOr, Value(0b1), 1).match(&chan));
    EXPECT_FALSE(ChannelMatch(ChannelMatch::ByteXor, Value(0b10100), 1).match(&chan));
    EXPECT_TRUE(ChannelMatch(ChannelMatch::ByteXor, Value(0b1), 1).match(&chan));

    EXPECT_FALSE(ChannelMatch().match(&chan));

    ChannelMatch match(ChannelMatch::Equal, Value(20), 1);
    EXPECT_TRUE(match.match(&chan));
    match.invert = true;
    EXPECT_FALSE(match.match(&chan));

    EXPECT_FALSE(ChannelMatch(ChannelMatch::Equal, Value(20), 42).match(&chan));

    EXPECT_TRUE(ChannelMatch(ChannelMatch::Equal, Value(20)).match(Value(20)));
    EXPECT_FALSE(ChannelMatch(ChannelMatch::Superior, Value(20)).match(Value(10)));
    EXPECT_FALSE(ChannelMatch().match(Value(20)));
    EXPECT_FALSE(ChannelMatch::equal(0).match(Value()));

    Channel bool_chan("bool_chan", "bool", 1);
    ASSERT_TRUE(bool_chan.write<bool>(0, true));
    EXPECT_TRUE(ChannelMatch::equal(true).match(&bool_chan));
    ChannelMatch not_false = ChannelMatch::equal(false);
    not_false.invert = true;
    EXPECT_TRUE(not_false.match(&bool_chan));

    Channel float_chan("float_chan", "float", 1);
    ASSERT_TRUE(float_chan.write<float>(0, 3.14f));
    EXPECT_TRUE(ChannelMatch::equal(3.14f).match(&float_chan));
    EXPECT_TRUE(ChannelMatch::superior(3.0f).match(&float_chan));
    EXPECT_FALSE(ChannelMatch::superior(4.0f).match(&float_chan));
}

TEST_F(TestChannelMatch, test_verify)
{
    Channel chan("chan", "int", 2);
    EXPECT_TRUE(ChannelMatch(ChannelMatch::Equal, Value(1)).verify(&chan));
    EXPECT_FALSE(ChannelMatch().verify(&chan));
    EXPECT_FALSE(ChannelMatch(ChannelMatch::Equal, Value(1), 5).verify(&chan));
    EXPECT_FALSE(ChannelMatch(ChannelMatch::Equal, Value(1.5)).verify(&chan));

    Channel float_chan("float_chan", "float", 1);
    EXPECT_TRUE(ChannelMatch(ChannelMatch::Equal, Value(1.5f)).verify(&float_chan));
}

TEST_F(TestChannelMatch, test_value_at)
{
    Channel chan("chan", "int", 3);
    ASSERT_TRUE(chan.write<int>(1, 25));
    EXPECT_TRUE(chan.value_at(1) == Value(25));
    EXPECT_FALSE(chan.value_at(0) == Value(25));
    EXPECT_FALSE(chan.value_at(3) == Value(25));
    EXPECT_TRUE(chan.value_at(3).empty());
    EXPECT_TRUE(chan.value_at(1) == chan.value_at(1));
}

} // namespace test
