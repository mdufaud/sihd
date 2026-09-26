#include <cmath>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include <sihd/util/TokenBucket.hpp>
#include <sihd/util/time.hpp>

#include "clock/clock_helper.hpp"

namespace test
{
using namespace sihd::util;

TEST(TestTokenBucket, test_token_bucket_consume)
{
    ManualClock clock;
    TokenBucket bucket(5.0, 1.0, Duration(time::milliseconds(100)));
    bucket.set_clock(&clock);

    EXPECT_DOUBLE_EQ(bucket.tokens(), 5.0);
    EXPECT_TRUE(bucket.try_consume());
    EXPECT_TRUE(bucket.try_consume(2.0));
    EXPECT_DOUBLE_EQ(bucket.tokens(), 2.0);
    EXPECT_FALSE(bucket.try_consume(3.0));
    EXPECT_TRUE(bucket.try_consume(2.0));
    EXPECT_DOUBLE_EQ(bucket.tokens(), 0.0);
    EXPECT_FALSE(bucket.try_consume(-1.0));
    EXPECT_FALSE(bucket.try_consume(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_DOUBLE_EQ(bucket.tokens(), 0.0);
    EXPECT_FALSE(bucket.try_consume());
}

TEST(TestTokenBucket, test_token_bucket_refill)
{
    ManualClock clock;
    TokenBucket bucket(2.0, 1.0, Duration(time::milliseconds(100)));
    bucket.set_clock(&clock);

    ASSERT_TRUE(bucket.try_consume(2.0));
    EXPECT_DOUBLE_EQ(bucket.tokens(), 0.0);

    clock.advance(time::milliseconds(50));
    EXPECT_DOUBLE_EQ(bucket.tokens(), 0.5);
    EXPECT_FALSE(bucket.try_consume(1.0));

    clock.advance(time::milliseconds(50));
    EXPECT_TRUE(bucket.try_consume(1.0));
    EXPECT_DOUBLE_EQ(bucket.tokens(), 0.0);

    // clamped at capacity
    clock.advance(time::hours(1));
    EXPECT_DOUBLE_EQ(bucket.tokens(), 2.0);
}

TEST(TestTokenBucket, test_token_bucket_time_for)
{
    ManualClock clock;
    TokenBucket bucket(10.0, 1.0, Duration(time::milliseconds(100)));
    bucket.set_clock(&clock);

    EXPECT_EQ(bucket.time_for(5.0), Duration(0));
    ASSERT_TRUE(bucket.try_consume(10.0));
    EXPECT_EQ(bucket.time_for(1.0), time::milliseconds(100));
    EXPECT_EQ(bucket.time_for(2.5), time::milliseconds(250));
}

TEST(TestTokenBucket, test_token_bucket_time_for_rounds_up)
{
    ManualClock clock;
    TokenBucket bucket(1.0, 3.0, Duration(10));
    bucket.set_clock(&clock);

    ASSERT_TRUE(bucket.try_consume());
    EXPECT_EQ(bucket.time_for(1.0), Duration(4));
    clock.advance(3);
    EXPECT_FALSE(bucket.try_consume());
    clock.advance(1);
    EXPECT_TRUE(bucket.try_consume());
}

TEST(TestTokenBucket, test_token_bucket_reset)
{
    ManualClock clock;
    TokenBucket bucket(3.0, 1.0, Duration(time::seconds(1)));
    bucket.set_clock(&clock);

    ASSERT_TRUE(bucket.try_consume(3.0));
    bucket.reset();
    EXPECT_DOUBLE_EQ(bucket.tokens(), 3.0);
    EXPECT_TRUE(bucket.try_consume(3.0));
}

TEST(TestTokenBucket, test_token_bucket_clock_switch)
{
    TokenBucket bucket(4.0, 1.0, Duration(time::milliseconds(100)));
    // full at construction on the default steady clock
    ManualClock clock(Timestamp(time::seconds(1000000)));
    // re-anchors on the new clock timeline
    bucket.set_clock(&clock);
    EXPECT_DOUBLE_EQ(bucket.tokens(), 4.0);

    ASSERT_TRUE(bucket.try_consume(4.0));
    clock.advance(time::milliseconds(100));
    EXPECT_TRUE(bucket.try_consume(1.0));
}

TEST(TestTokenBucket, test_token_bucket_invalid_tokens)
{
    ManualClock clock;
    TokenBucket bucket(2.0, 1.0, Duration(time::milliseconds(100)));
    bucket.set_clock(&clock);

    EXPECT_FALSE(bucket.try_consume(std::nan("")));
    EXPECT_FALSE(bucket.try_consume(-1.0));
    EXPECT_EQ(bucket.time_for(std::nan("")), Duration(std::numeric_limits<time::UnixTime>::max()));

    // backward clock jump: fail closed, no over-grant
    ASSERT_TRUE(bucket.try_consume(2.0));
    clock.advance(-time::milliseconds(50));
    EXPECT_DOUBLE_EQ(bucket.tokens(), 0.0);
}

TEST(TestTokenBucket, test_token_bucket_invalid)
{
    EXPECT_THROW(TokenBucket(0.0, 1.0, Duration(time::seconds(1))), std::invalid_argument);
    EXPECT_THROW(TokenBucket(-1.0, 1.0, Duration(time::seconds(1))), std::invalid_argument);
    EXPECT_THROW(TokenBucket(1.0, 0.0, Duration(time::seconds(1))), std::invalid_argument);
    EXPECT_THROW(TokenBucket(1.0, 1.0, Duration(0)), std::invalid_argument);
}

} // namespace test
