#include <gtest/gtest.h>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestWaitable: public ::testing::Test
{
    protected:
        TestWaitable() { sihd::util::LoggerManager::stream(); }

        virtual ~TestWaitable() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestWaitable, test_waitable_basic)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    Waitable waitable;
    std::atomic<bool> data = false;

    Synchronizer synchro_start(2);
    std::thread t([&]() {
        synchro_start.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto l = waitable.guard();
        data = true;
        waitable.notify(1);
    });
    synchro_start.sync();

    waitable.wait([&data] { return data == true; });

    t.join();
}

TEST_F(TestWaitable, test_waitable_for)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    Waitable waitable;
    std::atomic<bool> data = false;

    Synchronizer synchro_start(2);
    std::thread t([&]() {
        synchro_start.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto l = waitable.guard();
        data = true;
        waitable.notify(1);
    });
    synchro_start.sync();

    bool condition_ok = waitable.wait_for(std::chrono::milliseconds(50), [&data] { return data == true; });
    EXPECT_TRUE(condition_ok);

    t.join();
}

TEST_F(TestWaitable, test_waitable_until)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    SystemClock clock;
    Waitable waitable;
    std::atomic<bool> data = false;

    Synchronizer synchro_start(2);
    std::thread t([&]() {
        synchro_start.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto l = waitable.guard();
        data = true;
        waitable.notify(1);
    });
    synchro_start.sync();

    bool condition_ok = waitable.wait_until(Timestamp(clock.now() + time::ms(50)), [&data] { return data == true; });
    EXPECT_TRUE(condition_ok);
    t.join();
}

TEST_F(TestWaitable, test_waitable_for_elapsed)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Waitable waitable;
    std::atomic<bool> data = false;

    Synchronizer synchro_start(2);
    std::thread t([&]() {
        synchro_start.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        {
            auto l = waitable.guard();
            data = true;
        }
        waitable.notify(1);
    });
    synchro_start.sync();

    Duration elapsed = waitable.wait_for_elapsed(std::chrono::milliseconds(50), [&data] { return data == true; });
    EXPECT_LE(elapsed.milliseconds(), 50);

    t.join();
}

TEST_F(TestWaitable, test_waitable_elapsed_timeouts)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    Waitable waitable;
    SystemClock clock;

    Duration waited_for = waitable.wait_for_elapsed(std::chrono::milliseconds(20), [] { return false; });
    EXPECT_GE(waited_for.milliseconds(), 15);

    const Timestamp deadline(clock.now() + time::ms(20));
    Duration waited_until = waitable.wait_until_elapsed(deadline, [] { return false; });
    EXPECT_GE(waited_until.milliseconds(), 15);
}

TEST_F(TestWaitable, test_waitable_guard_timeout)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    Waitable waitable;

    auto guard = waitable.wait_for_guard(std::chrono::milliseconds(10), [] { return false; });
    EXPECT_TRUE(guard.timed_out);
}

TEST_F(TestWaitable, test_waitable_notify_all_wakes_every_waiter)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    Waitable waitable;
    std::atomic<int> awakened = 0;
    std::vector<std::thread> waiters;

    for (size_t i = 0; i < 3; ++i)
    {
        waiters.emplace_back([&] {
            if (waitable.wait_for(std::chrono::milliseconds(100)) == false)
                ++awakened;
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    waitable.notify_all();

    for (std::thread & t : waiters)
        t.join();

    EXPECT_EQ(awakened.load(), 3);
}

} // namespace test
