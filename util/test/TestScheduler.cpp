#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/time.hpp>

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
                std::time_t ms = duration_cast<milliseconds>(now - _last).count();
                if (_last.time_since_epoch().count() == 0)
                    _last = now;
                else
                {
                    if (should_run_every_ms != ms)
                        good_freq = false;
                    TRACE("Time since last call: " << ms << " ms");
                    _last = now;
                }
                ran += 1;
                return true;
            }

            bool good_freq = true;
            int ran = 0;
            int should_run_every_ms = 2;
            time_point<steady_clock, nanoseconds> _last;
            steady_clock _clock;
    };

    TEST_F(TestScheduler, test_seq_perf)
    {
        Scheduler seq("seq");
        SystemClock clock;

        int lambda_ran = 0;
        seq.add_task(new Task([&] () -> bool
        {
            ++lambda_ran;
            return true;
        }, 0));

        this->should_run_every_ms = 3;
        seq.add_task(new Task(this, 0, time::milli(this->should_run_every_ms)));
        seq.start();
        std::time_t sleep_time = 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        seq.stop();
        EXPECT_EQ(lambda_ran, 1);
        EXPECT_EQ(this->ran, sleep_time / this->should_run_every_ms);
        EXPECT_EQ(this->good_freq, true);
        EXPECT_EQ(seq.overruns, 2u);
    }

    TEST_F(TestScheduler, test_seq_stop)
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
}
