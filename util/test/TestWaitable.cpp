#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/Clocks.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestWaitable:   public ::testing::Test
    {
        protected:
            TestWaitable()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestWaitable()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestWaitable, test_waitable_elapsed)
    {
        if (os::is_run_by_valgrind())
            GTEST_SKIP() << "Buggy with valgrind";
        Waitable waitable;

        std::thread t([&] ()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            waitable.notify(1);
        });
        time_t elapsed = waitable.wait_for_elapsed(time::milli(5));
        t.join();
        EXPECT_EQ(time::to_milli(elapsed), 2);
    }

    TEST_F(TestWaitable, test_waitable_loop)
    {
        if (os::is_run_by_valgrind())
            GTEST_SKIP() << "Buggy with valgrind";
        Waitable waitable;
        SteadyClock clock;

        time_t now = clock.now();
        std::thread t([&] ()
        {
            int i = 0;
            while (i < 3)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(330));
                waitable.notify(1);
                ++i;
            }
        });
        bool timeout = waitable.wait_for_loop(time::milli(10), 3);
        t.join();
        EXPECT_EQ(timeout, false);
        EXPECT_EQ(time::to_milli(clock.now() - now), 1);
    }

    TEST_F(TestWaitable, test_waitable_loop_fail)
    {
        if (os::is_run_by_valgrind())
            GTEST_SKIP() << "Buggy with valgrind";
        Waitable waitable;
        SteadyClock clock;

        time_t now = clock.now();
        std::thread t([&] ()
        {
            int i = 0;
            while (i < 3)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(330));
                waitable.notify(1);
                ++i;
            }
        });
        bool timeout = waitable.wait_for_loop(time::milli(5), 4);
        t.join();
        EXPECT_EQ(timeout, true);
        EXPECT_EQ(time::to_milli(clock.now() - now), 5);
    }

}
