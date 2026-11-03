#include <gtest/gtest.h>

#include <sihd/sys/SleepWatcher.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestSleepWatcher: public ::testing::Test
{
    protected:
        TestSleepWatcher() { LoggerManager::stream(); }

        virtual ~TestSleepWatcher() { LoggerManager::clear_loggers(); }
};

TEST_F(TestSleepWatcher, test_lifecycle)
{
    SleepWatcher watcher;
    const SleepWatcher::Callback callback = [](SleepWatcher::Event) {
    };

    auto started = watcher.start(callback);

    ASSERT_TRUE(started.has_value()) << started.error().message;
    EXPECT_TRUE(watcher.is_running());

    auto twice = watcher.start(callback);

    ASSERT_FALSE(twice.has_value());
    EXPECT_EQ(twice.error().code, ErrorCode::already_exists);
    EXPECT_EQ(twice.error().message, "sleep watcher is already running");

    auto stopped = watcher.stop();

    EXPECT_TRUE(stopped.has_value()) << stopped.error().message;
    EXPECT_FALSE(watcher.is_running());
    EXPECT_TRUE(watcher.stop().has_value());
}

TEST_F(TestSleepWatcher, test_empty_callback)
{
    SleepWatcher watcher;

    auto started = watcher.start({});

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(started.error().message, "a sleep watcher needs a callback");
}

} // namespace test
