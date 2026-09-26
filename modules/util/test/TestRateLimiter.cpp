#include <stdexcept>

#include <gtest/gtest.h>

#include <sihd/util/RateLimiter.hpp>
#include <sihd/util/time.hpp>

#include "clock/clock_helper.hpp"

namespace test
{
using namespace sihd::util;

TEST(TestRateLimiter, test_rate_limiter_burst)
{
    ManualClock clock;
    RateLimiter limiter(2, Duration(time::seconds(1)));
    limiter.set_clock(&clock);

    EXPECT_TRUE(limiter.allow());
    EXPECT_TRUE(limiter.allow());
    EXPECT_FALSE(limiter.allow());
    // one event refilled halfway through the window
    EXPECT_EQ(limiter.retry_after(), time::milliseconds(500));
}

TEST(TestRateLimiter, test_rate_limiter_refill)
{
    ManualClock clock;
    RateLimiter limiter(2, Duration(time::seconds(1)));
    limiter.set_clock(&clock);

    ASSERT_TRUE(limiter.allow());
    ASSERT_TRUE(limiter.allow());
    EXPECT_FALSE(limiter.allow());

    clock.advance(time::milliseconds(500));
    EXPECT_TRUE(limiter.allow());
    EXPECT_FALSE(limiter.allow());

    clock.advance(time::milliseconds(250));
    EXPECT_FALSE(limiter.allow());
    clock.advance(time::milliseconds(250));
    EXPECT_TRUE(limiter.allow());
}

TEST(TestRateLimiter, test_rate_limiter_reset)
{
    ManualClock clock;
    RateLimiter limiter(1, Duration(time::seconds(1)));
    limiter.set_clock(&clock);

    ASSERT_TRUE(limiter.allow());
    EXPECT_FALSE(limiter.allow());
    limiter.reset();
    EXPECT_TRUE(limiter.allow());
}

TEST(TestRateLimiter, test_rate_limiter_full)
{
    ManualClock clock;
    RateLimiter limiter(2, Duration(time::seconds(1)));
    limiter.set_clock(&clock);

    EXPECT_TRUE(limiter.full());
    ASSERT_TRUE(limiter.allow());
    EXPECT_FALSE(limiter.full());
    clock.advance(time::seconds(1));
    EXPECT_TRUE(limiter.full());
}

TEST(TestRateLimiter, test_rate_limiter_invalid)
{
    EXPECT_THROW(RateLimiter(0, Duration(time::seconds(1))), std::invalid_argument);
}

} // namespace test
