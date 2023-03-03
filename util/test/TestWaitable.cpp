#include <gtest/gtest.h>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestWaitable: public ::testing::Test
{
    protected:
        TestWaitable() { sihd::util::LoggerManager::basic(); }

        virtual ~TestWaitable() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestWaitable, test_waitable_elapsed)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Waitable waitable;

    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        waitable.notify(1);
    });
    time_t elapsed = waitable.wait_for_elapsed(time::milli(5));
    t.join();
    EXPECT_NEAR(time::to_milli(elapsed), 4, 2);
}

TEST_F(TestWaitable, test_waitable_until_predicate)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    SteadyClock clock;
    Waitable waitable;
    int max_calls = 4;
    int calls = 0;

    Timestamp timeout = std::chrono::milliseconds(6);
    Timestamp poll_frequency = std::chrono::milliseconds(1);

    time_t before = clock.now();
    Timestamp elapsed = waitable.infinite_wait_until_predicate(
        [&calls, max_calls] {
            ++calls;
            return calls == max_calls;
        },
        poll_frequency);
    time_t after = clock.now();

    Timestamp calculated_elapsed = after - before;

    EXPECT_EQ(calls, max_calls);
    EXPECT_LT(elapsed, timeout);
    EXPECT_LT(calculated_elapsed, timeout);

    max_calls = 100;
    before = clock.now();
    elapsed = waitable.wait_until_predicate(
        timeout,
        [&calls, max_calls] {
            ++calls;
            return calls == max_calls;
        },
        poll_frequency);
    after = clock.now();
    calculated_elapsed = after - before;

    EXPECT_GE(elapsed, timeout);
    EXPECT_GE(calculated_elapsed, timeout);
}

TEST_F(TestWaitable, test_waitable_loop)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Waitable waitable;
    SteadyClock clock;
    Synchronizer synchro;

    synchro.init_sync(2);
    time_t before = clock.now();
    std::thread t([&]() {
        synchro.sync();
        for (int i = 0; i < 3; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            waitable.notify(1);
        }
    });
    synchro.sync();
    bool timeout = waitable.wait_for_loop(time::milli(15), 3);
    t.join();
    EXPECT_EQ(timeout, false);
    EXPECT_LE(time::to_milli(clock.now() - before), 14);
}

TEST_F(TestWaitable, test_waitable_loop_fail)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Waitable waitable;
    SteadyClock clock;
    Synchronizer synchro;

    synchro.init_sync(2);
    time_t now = clock.now();
    std::thread t([&]() {
        synchro.sync();
        for (int i = 0; i < 3; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            waitable.notify(1);
        }
    });
    synchro.sync();
    bool timeout = waitable.wait_for_loop(time::milli(5), 4);
    t.join();
    EXPECT_EQ(timeout, true);
    EXPECT_LE(time::to_milli(clock.now() - now), 8);
}

} // namespace test
