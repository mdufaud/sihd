#include <thread>

#include <gtest/gtest.h>

#include <sihd/sys/Poll.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)

# include <sihd/sys/linux/EventFd.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_;

class TestEventFd: public ::testing::Test
{
    protected:
        TestEventFd() { LoggerManager::stream(); }

        virtual ~TestEventFd() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!EventFd::supported)
            {
                GTEST_SKIP() << "eventfd not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestEventFd, test_cross_thread_notify)
{
    EventFd event;
    ASSERT_TRUE(event.open());
    Poll poll(1);
    ASSERT_TRUE(poll.set_read_fd(event));

    std::jthread notifier([&event] { (void)event.notify(3); });
    EXPECT_EQ(poll.poll(1000), 1);
    notifier.join();

    auto count = event.read();
    ASSERT_TRUE(count.has_value()) << count.error().message;
    EXPECT_EQ(*count, 3u);
    auto drained = event.read();
    ASSERT_TRUE(drained.has_value());
    EXPECT_EQ(*drained, 0u);
}

TEST_F(TestEventFd, test_counter_accumulates)
{
    EventFd event;
    ASSERT_TRUE(event.open());

    ASSERT_TRUE(event.notify());
    ASSERT_TRUE(event.notify(41));

    auto count = event.read();
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count, 42u);
}

TEST_F(TestEventFd, test_errors)
{
    EventFd event;
    auto closed = event.notify();
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error().code, ErrorCode::not_initialized);

    ASSERT_TRUE(event.open());
    ASSERT_TRUE(event.notify(0xfffffffffffffffeull));
    auto saturated = event.notify(1);
    ASSERT_FALSE(saturated.has_value());
    EXPECT_EQ(saturated.error().code, ErrorCode::would_block);
}

TEST_F(TestEventFd, test_move)
{
    EventFd event;
    ASSERT_TRUE(event.open());
    const int fd = event.fd();

    EventFd moved(std::move(event));
    EXPECT_FALSE(event.is_open());
    EXPECT_EQ(moved.fd(), fd);

    moved.close();
    EXPECT_FALSE(moved.is_open());
}

} // namespace test

#endif
