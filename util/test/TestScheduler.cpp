#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/OS.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    using namespace std::chrono;

    class TestScheduler:    public ::testing::Test,
                            public IRunnable
    {
        protected:
            TestScheduler()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestScheduler()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            virtual bool run()
            {
                time_point<steady_clock, nanoseconds> now = _clock.now();
                std::time_t usec = duration_cast<microseconds>(now - _last).count();
                if (_last.time_since_epoch().count() == 0)
                    _last = now;
                else
                {
                    if ((usec < (this->should_run_every_us + this->delta_us)
                            && usec > (this->should_run_every_us - this->delta_us)) == false)
                    {
                        SIHD_CERR("Overrun: " << usec << " usec since last run (max: "
                                    << this->should_run_every_us + this->delta_us << " usec)");
                        this->good_freq = false;
                    }
                    _last += std::chrono::microseconds(this->should_run_every_us);
                }
                ran += 1;
                return true;
            }

            std::time_t delta_us = 0;
            bool good_freq = true;
            int ran = 0;
            int should_run_every_us = 0;
            time_point<steady_clock, nanoseconds> _last;
            steady_clock _clock;
    };

    TEST_F(TestScheduler, test_sched_perf)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Perf under valgrind debugger is unthinkable";

        Scheduler seq("seq");
        // most overruns are below 100 microseconds
        this->delta_us = 100;
        seq.overrun_at = time::micro(this->delta_us);
        int lambda_ran = 0;
        seq.add_task(new Task([&lambda_ran] () -> bool
        {
            ++lambda_ran;
            return true;
        }, 0));

        this->should_run_every_us = 100;
        seq.add_task(new Task(this, 0, time::micro(this->should_run_every_us)));
        seq.start();
        std::time_t sleep_time = 50;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        seq.stop();
        EXPECT_EQ(lambda_ran, 1);
        int expected_ran = time::micro(sleep_time) / this->should_run_every_us;
        EXPECT_NEAR(this->ran, expected_ran, 3);
        SIHD_LOG(info, "Scheduler total tasks executed: " << this->ran);
        SIHD_LOG(info, "Scheduler total overruns: " << seq.overruns);
        // 10% miss maximum - generally is around 2%
        EXPECT_LT(seq.overruns, (this->ran / 10));
    }

    TEST_F(TestScheduler, test_sched_stop)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Valgrind debugger doesn't hold up there";

        Scheduler seq("seq");
        steady_clock steady_clock;

        int ran = 0;
        auto before = steady_clock.now();
        std::time_t now = seq.now();
        seq.add_task(new Task([&] () -> bool
        {
            SIHD_TRACE("Should run once");
            ++ran;
            return true;
        }, now + time::milli(5)));
        seq.add_task(new Task([&] () -> bool
        {
            SIHD_TRACE("Should not run");
            ++ran;
            return true;
        }, now + time::milli(10)));
        seq.start();
        SIHD_TRACE("Before sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
        EXPECT_EQ(ran, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        EXPECT_EQ(ran, 1);
        seq.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        EXPECT_EQ(ran, 1);
        auto after = steady_clock.now();
        std::time_t diff_ms = duration_cast<milliseconds>(after - before).count();
        EXPECT_LE(diff_ms, 17);
    }

    TEST_F(TestScheduler, test_sched_pause)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Test is buggy under valgrind debugger";
        Scheduler seq("seq");

        int lambda_ran = 0;
        // 1 ms
        time_t should_run_every_ms = 1;
        seq.add_task(new Task([&lambda_ran] () -> bool
        {
            ++lambda_ran;
            return true;
        }, 0, time::ms(should_run_every_ms)));
        std::time_t sleep_ms = 10;
        seq.start();
        SIHD_LOG(debug, "Started scheduler");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        SIHD_LOG(debug, "Pausing scheduler");
        seq.pause();
        SIHD_LOG(debug, "Paused scheduler");
        EXPECT_NEAR(lambda_ran, (sleep_ms / should_run_every_ms) + 1, 1);
        EXPECT_EQ(seq.overruns, 0u);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        EXPECT_NEAR(lambda_ran, (sleep_ms / should_run_every_ms) + 1, 1);
        EXPECT_EQ(seq.overruns, 0u);
        SIHD_LOG(debug, "Resuming scheduler");
        seq.resume();
        SIHD_LOG(debug, "Resumed scheduler");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        SIHD_LOG(debug, "Pausing scheduler");
        seq.pause();
        SIHD_LOG(debug, "Paused scheduler");
        EXPECT_NEAR(lambda_ran, ((2 * sleep_ms) / should_run_every_ms) + 1, 1);
        EXPECT_EQ(seq.overruns, 0u);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        EXPECT_NEAR(lambda_ran, ((2 * sleep_ms) / should_run_every_ms) + 1, 1);
        EXPECT_EQ(seq.overruns, 0u);
        SIHD_LOG(debug, "Resuming scheduler");
        seq.resume();
        SIHD_LOG(debug, "Resumed scheduler");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        SIHD_LOG(debug, "Stopping scheduler");
        seq.stop();
        SIHD_LOG(debug, "Stopped scheduler");
        EXPECT_NEAR(lambda_ran, ((3 * sleep_ms) / should_run_every_ms) + 1, 1);
        EXPECT_EQ(seq.overruns, 0u);
    }

    TEST_F(TestScheduler, test_sched_as_fast)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Test is buggy under valgrind debugger";
        Scheduler seq("seq");
        int lambda_ran = 0;
        std::function<bool()> fun = [&lambda_ran] () -> bool
        {
            ++lambda_ran;
            return true;
        };
        time_t now = seq.now();
        seq.add_task(new Task(fun, now + time::milli(1)));
        seq.add_task(new Task(fun, now + time::milli(5)));
        seq.add_task(new Task(fun, now + time::milli(10)));
        seq.add_task(new Task(fun, now + time::milli(15)));
        seq.add_task(new Task(fun, now + time::milli(20)));
        seq.set_as_fast_as_possible(true);
        seq.start();
        SIHD_LOG(debug, "Started scheduler");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        seq.stop();
        SIHD_LOG(debug, "Stopped scheduler");
        EXPECT_EQ(lambda_ran, 5);
    }

    TEST_F(TestScheduler, test_sched_burst)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Test is buggy under valgrind debugger";
        Scheduler seq("seq");
        int lambda_ran = 0;
        std::function<bool()> fun = [&lambda_ran] () -> bool
        {
            ++lambda_ran;
            return true;
        };
        seq.add_task(new Task(fun, 0, 1));
        int i = 0;
        while (i < 100)
        {
            seq.start();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            seq.pause();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            seq.resume();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            seq.stop();
            ++i;
        }
    }
}