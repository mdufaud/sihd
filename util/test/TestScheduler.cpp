#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/OS.hpp>

namespace test
{
    LOGGER;
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
                std::time_t us = duration_cast<microseconds>(now - _last).count();
                if (_last.time_since_epoch().count() == 0)
                    _last = now;
                else
                {
                    std::time_t micro = time::to_micro(should_run_every_us);
                    if (micro > (us + delta)
                        || micro < (us - delta))
                        good_freq = false;
                    //TRACE("Time since last call: " << us << " us");
                    _last = now;
                }
                ran += 1;
                return true;
            }

            std::time_t delta = time::micro(100);
            bool good_freq = true;
            int ran = 0;
            int should_run_every_us = 2000;
            time_point<steady_clock, nanoseconds> _last;
            steady_clock _clock;
    };

    TEST_F(TestScheduler, test_sched_perf)
    {
        Scheduler seq("seq");

        int lambda_ran = 0;
        seq.add_task(new Task([&lambda_ran] () -> bool
        {
            ++lambda_ran;
            return true;
        }, 0));

        this->should_run_every_us = 60;
        seq.add_task(new Task(this, 0, time::micro(this->should_run_every_us)));
        seq.start();
        std::time_t sleep_time = 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        seq.stop();
        EXPECT_EQ(lambda_ran, 1);
        int expected_ran = time::micro(sleep_time) / this->should_run_every_us;
        EXPECT_NEAR(this->ran, expected_ran, 3);
        EXPECT_EQ(seq.overruns, 2u);
        EXPECT_EQ(this->good_freq, true);
    }

    TEST_F(TestScheduler, test_sched_stop)
    {
        Scheduler seq("seq");
        SystemClock clock;
        steady_clock steady_clock;

        int ran = 0;
        auto before = steady_clock.now();
        std::time_t now = clock.now();
        seq.add_task(new Task([&] () -> bool
        {
            TRACE("Should run once");
            ++ran;
            return true;
        }, now + time::milli(10)));
        seq.add_task(new Task([&] () -> bool
        {
            TRACE("Should not run");
            ++ran;
            return true;
        }, now + time::sec(1)));
        seq.start();
        TRACE("Before sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(9));
        EXPECT_EQ(ran, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        EXPECT_EQ(ran, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        TRACE("After sleep");
        seq.stop();
        EXPECT_EQ(ran, 1);
        auto after = steady_clock.now();
        std::time_t diff_ms = duration_cast<milliseconds>(after - before).count();
        EXPECT_LE(diff_ms, 65);
    }

    TEST_F(TestScheduler, test_sched_pause)
    {
        if (OS::is_run_by_valgrind())
            GTEST_SKIP() << "Test is buggy under valgrind debugger";
            
        Scheduler seq("seq");
        SystemClock clock;

        int lambda_ran = 0;
        // 1 ms
        time_t should_run_every_us = 1000;
        seq.add_task(new Task([&lambda_ran] () -> bool
        {
            ++lambda_ran;
            return true;
        }, clock.now() + 100, time::micro(should_run_every_us)));
        std::time_t sleep_time = 10;
        seq.start();
        LOG(debug, "Started scheduler");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        LOG(debug, "Pausing scheduler");
        seq.pause();
        LOG(debug, "Paused scheduler");
        EXPECT_NEAR(lambda_ran, 11, 1);
        EXPECT_EQ(seq.overruns, 0u);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        EXPECT_NEAR(lambda_ran, 11, 1);
        EXPECT_EQ(seq.overruns, 0u);
        LOG(debug, "Resuming scheduler");
        seq.resume();
        LOG(debug, "Resumed scheduler");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        LOG(debug, "Pausing scheduler");
        seq.pause();
        LOG(debug, "Paused scheduler");
        EXPECT_NEAR(lambda_ran, 21, 1);
        EXPECT_EQ(seq.overruns, 0u);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        EXPECT_NEAR(lambda_ran, 21, 1);
        EXPECT_EQ(seq.overruns, 0u);
        LOG(debug, "Resuming scheduler");
        seq.resume();
        LOG(debug, "Resumed scheduler");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        LOG(debug, "Stopping scheduler");
        seq.stop();
        LOG(debug, "Stopped scheduler");
        EXPECT_NEAR(lambda_ran, 31, 1);
        EXPECT_EQ(seq.overruns, 0u);
    }

    TEST_F(TestScheduler, test_sched_as_fast)
    {
        if (OS::is_run_by_valgrind())
        {
            GTEST_SKIP() << "Test is buggy under valgrind debugger";
        }
        Scheduler seq("seq");
        SystemClock clock;
        int lambda_ran = 0;
        std::function<bool()> fun = [&lambda_ran] () -> bool
        {
            TRACE("PLAY")
            ++lambda_ran;
            return true;
        };
        time_t now = clock.now();
        seq.add_task(new Task(fun, now + time::milli(1)));
        seq.add_task(new Task(fun, now + time::milli(5)));
        seq.add_task(new Task(fun, now + time::milli(10)));
        seq.add_task(new Task(fun, now + time::milli(15)));
        seq.add_task(new Task(fun, now + time::milli(20)));
        seq.set_as_fast_as_possible(true);
        std::time_t milli_sleep = 7;
        seq.start();
        LOG(debug, "Started scheduler");
        std::this_thread::sleep_for(std::chrono::milliseconds(milli_sleep));
        seq.stop();
        LOG(debug, "Stopped scheduler");
        EXPECT_EQ(lambda_ran, 5);
    }
}