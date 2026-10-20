#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "watchdog/test_watchdog.hpp"

namespace test
{

class TestTestWatchdog: public ::testing::Test
{
    protected:
        TestTestWatchdog() = default;
        virtual ~TestTestWatchdog() = default;

        static const ::testing::TestInfo & current()
        {
            return *::testing::UnitTest::GetInstance()->current_test_info();
        }
};

TEST_F(TestTestWatchdog, test_watchdog_rearms_on_each_test)
{
    TestWatchdog watchdog(std::chrono::seconds(1));

    for (int i = 0; i < 3; ++i)
    {
        watchdog.OnTestStart(current());
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        watchdog.OnTestEnd(current());
    }
}

#if GTEST_HAS_DEATH_TEST
TEST_F(TestTestWatchdog, test_watchdog_names_the_hung_test)
{
    EXPECT_DEATH(
        {
            TestWatchdog watchdog(std::chrono::seconds(1));
            watchdog.OnTestStart(current());
            std::this_thread::sleep_for(std::chrono::seconds(30));
        },
        "TestTestWatchdog.test_watchdog_names_the_hung_test timed out after 1s");
}
#endif

} // namespace test
