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

TEST_F(TestWaitable, test_waitable_basic)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    Waitable waitable;
    std::atomic<bool> data = false;

    Synchronizer synchro;
    synchro.init_sync(2);
    std::thread t([&]() {
        synchro.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto l = waitable.guard();
        data = true;
        waitable.notify(1);
    });
    synchro.sync();

    waitable.wait([&data] { return data == true; });

    t.join();
}

TEST_F(TestWaitable, test_waitable_for)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    Waitable waitable;
    std::atomic<bool> data = false;

    Synchronizer synchro;
    synchro.init_sync(2);
    std::thread t([&]() {
        synchro.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto l = waitable.guard();
        data = true;
        waitable.notify(1);
    });
    synchro.sync();

    bool condition_ok = waitable.wait_for(std::chrono::milliseconds(5), [&data] { return data == true; });
    EXPECT_TRUE(condition_ok);

    t.join();
}

TEST_F(TestWaitable, test_waitable_until)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";

    SystemClock clock;
    Waitable waitable;
    std::atomic<bool> data = false;

    Synchronizer synchro;
    synchro.init_sync(2);
    std::thread t([&]() {
        synchro.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto l = waitable.guard();
        data = true;
        waitable.notify(1);
    });
    synchro.sync();

    bool condition_ok = waitable.wait_until(clock.now() + time::ms(5), [&data] { return data == true; });
    EXPECT_TRUE(condition_ok);
    t.join();
}

TEST_F(TestWaitable, test_waitable_for_elapsed)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Waitable waitable;
    std::atomic<bool> data = false;

    Synchronizer synchro;
    synchro.init_sync(2);
    std::thread t([&]() {
        synchro.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto l = waitable.guard();
        data = true;
        waitable.notify(1);
    });
    synchro.sync();

    Timestamp elapsed = waitable.wait_for_elapsed(std::chrono::milliseconds(5), [&data] { return data == true; });
    EXPECT_LE(elapsed.milliseconds(), 5);

    t.join();
}

} // namespace test
