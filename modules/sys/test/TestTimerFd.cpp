#include <gtest/gtest.h>

#include <sihd/sys/Poll.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

#if defined(__SIHD_LINUX__)

# include <sihd/sys/linux/TimerFd.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;

class TestTimerFd: public ::testing::Test
{
    protected:
        TestTimerFd() { LoggerManager::stream(); }

        virtual ~TestTimerFd() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!TimerFd::supported)
            {
                GTEST_SKIP() << "timerfd not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestTimerFd, test_expires_in_poll)
{
    TimerFd timer;
    ASSERT_TRUE(timer.open());
    ASSERT_TRUE(timer.is_open());
    Poll poll(1);
    ASSERT_TRUE(poll.set_read_fd(timer));

    ASSERT_TRUE(timer.set(Duration(time::milli(20))));
    EXPECT_EQ(poll.poll(1000), 1);

    auto expirations = timer.wait();
    ASSERT_TRUE(expirations.has_value()) << expirations.error().message;
    EXPECT_EQ(*expirations, 1u);
    auto drained = timer.wait();
    ASSERT_TRUE(drained.has_value());
    EXPECT_EQ(*drained, 0u);
}

TEST_F(TestTimerFd, test_periodic_and_disarm)
{
    TimerFd timer;
    ASSERT_TRUE(timer.open());
    Poll poll(1);
    ASSERT_TRUE(poll.set_read_fd(timer));

    ASSERT_TRUE(timer.set(Duration(time::milli(5)), Duration(time::milli(5))));
    EXPECT_EQ(poll.poll(1000), 1);
    auto first = timer.wait();
    ASSERT_TRUE(first.has_value());
    EXPECT_GE(*first, 1u);
    EXPECT_EQ(poll.poll(1000), 1);
    auto second = timer.wait();
    ASSERT_TRUE(second.has_value());
    EXPECT_GE(*second, 1u);

    ASSERT_TRUE(timer.disarm());
    ASSERT_TRUE(timer.wait().has_value());
    EXPECT_EQ(poll.poll(30), 0);
}

TEST_F(TestTimerFd, test_errors)
{
    TimerFd timer;
    auto closed = timer.set(Duration(time::milli(1)));
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error().code, ErrorCode::not_initialized);

    ASSERT_TRUE(timer.open());
    auto zero = timer.set(Duration(0));
    ASSERT_FALSE(zero.has_value());
    EXPECT_EQ(zero.error().code, ErrorCode::invalid_argument);
}

TEST_F(TestTimerFd, test_move)
{
    TimerFd timer;
    ASSERT_TRUE(timer.open());
    const int fd = timer.fd();

    TimerFd moved(std::move(timer));
    EXPECT_FALSE(timer.is_open());
    EXPECT_EQ(moved.fd(), fd);

    timer = std::move(moved);
    EXPECT_EQ(timer.fd(), fd);
    EXPECT_FALSE(moved.is_open());

    timer.close();
    EXPECT_FALSE(timer.is_open());
    EXPECT_EQ(timer.fd(), -1);
}

} // namespace test

#endif
