#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace std::chrono;

class TestScheduler: public ::testing::Test,
                     public IRunnable
{
    protected:
        TestScheduler() { sihd::util::LoggerManager::basic(); }

        virtual ~TestScheduler() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        virtual bool run()
        {
            time_point<steady_clock, nanoseconds> now = _clock.now();
            time_t usec = duration_cast<microseconds>(now - _last).count();
            if (_last.time_since_epoch().count() == 0)
                _last = now;
            else
            {
                if ((usec < (this->should_run_every_us + this->delta_us)
                     && usec > (this->should_run_every_us - this->delta_us))
                    == false)
                {
                    SIHD_CERR("Overrun: {} usec since last run (max: {} usec)\n",
                              usec,
                              this->should_run_every_us + this->delta_us);
                    this->good_freq = false;
                    overruns++;
                }
                _last += std::chrono::microseconds(this->should_run_every_us);
            }
            ran += 1;
            return true;
        }

        time_t delta_us = 0;
        bool good_freq = true;
        size_t overruns = 0;
        int ran = 0;
        int should_run_every_us = 0;
        time_point<steady_clock, nanoseconds> _last;
        steady_clock _clock;
};

TEST_F(TestScheduler, test_sched_perf)
{
    // if (os::is_run_by_valgrind())
    //     GTEST_SKIP() << "Perf under valgrind debugger is unthinkable";

    Scheduler seq("seq");

    // most overruns are below 100 microseconds
    this->delta_us = 100;
    seq.overrun_at = time::micro(this->delta_us);

    this->should_run_every_us = 100;
    seq.add_task(new Task(this, 0, std::chrono::microseconds(this->should_run_every_us)));
    seq.start();

    constexpr time_t sleep_time = 50;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

    seq.stop();

    const int expected_ran = time::micro(sleep_time) / this->should_run_every_us;
    const int minimum_ran = expected_ran / 2;

    auto level = num::near(this->ran, expected_ran, 1) ? LogLevel::info : LogLevel::error;

    SIHD_LOG_LVL(level, "Scheduler tasks executed: total={} expected={}", this->ran, expected_ran);

    // 10% miss maximum - generally is around 2%
    const int expected_overruns = this->ran / 10;
    SIHD_LOG_LVL(level,
                 "Scheduler overruns: total={} calculated={} expected_less={}",
                 seq.overruns,
                 overruns,
                 expected_overruns);

    EXPECT_LE(seq.overruns, minimum_ran);
    EXPECT_GT(this->ran, minimum_ran);
}

TEST_F(TestScheduler, test_sched_stop)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Valgrind debugger doesn't hold up there";

    Scheduler seq("seq");
    steady_clock steady_clock;

    int ran = 0;
    auto before = steady_clock.now();
    time_t now = seq.now();
    seq.add_task(new Task(
        [&]() -> bool {
            SIHD_TRACE("Should run once");
            ++ran;
            return true;
        },
        now + time::milli(5)));
    seq.add_task(new Task(
        [&]() -> bool {
            SIHD_TRACE("Should not run");
            ++ran;
            return true;
        },
        now + time::milli(10)));
    seq.start();
    SIHD_TRACE("Before sleep");
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    EXPECT_EQ(ran, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_EQ(ran, 1);
    seq.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(ran, 1);
    auto after = steady_clock.now();
    time_t diff_ms = duration_cast<milliseconds>(after - before).count();
    EXPECT_LE(diff_ms, 20);
}

TEST_F(TestScheduler, test_sched_pause)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Test is buggy under valgrind debugger";

    constexpr time_t should_run_every_ms = 5;
    constexpr time_t sleep_ms = 20;

    Scheduler seq("seq");

    int lambda_ran = 0;
    // 1 ms
    seq.add_task(new Task(
        [&lambda_ran]() -> bool {
            ++lambda_ran;
            return true;
        },
        0,
        time::ms(should_run_every_ms)));
    seq.start();
    SIHD_LOG(debug, "Started scheduler");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    SIHD_LOG(debug, "Pausing scheduler");
    seq.pause();
    SIHD_LOG(debug, "Paused scheduler");
    EXPECT_NEAR(lambda_ran, (sleep_ms / should_run_every_ms) + 1, 1);
    EXPECT_NEAR(seq.overruns, 0u, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    EXPECT_NEAR(lambda_ran, (sleep_ms / should_run_every_ms) + 1, 1);
    EXPECT_NEAR(seq.overruns, 0u, 1);
    SIHD_LOG(debug, "Resuming scheduler");
    seq.resume();
    SIHD_LOG(debug, "Resumed scheduler");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    SIHD_LOG(debug, "Pausing scheduler");
    seq.pause();
    SIHD_LOG(debug, "Paused scheduler");
    EXPECT_NEAR(lambda_ran, ((2 * sleep_ms) / should_run_every_ms) + 1, 1);
    EXPECT_NEAR(seq.overruns, 0u, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    EXPECT_NEAR(lambda_ran, ((2 * sleep_ms) / should_run_every_ms) + 1, 1);
    EXPECT_NEAR(seq.overruns, 0u, 1);
    SIHD_LOG(debug, "Resuming scheduler");
    seq.resume();
    SIHD_LOG(debug, "Resumed scheduler");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    SIHD_LOG(debug, "Stopping scheduler");
    seq.stop();
    SIHD_LOG(debug, "Stopped scheduler");
    EXPECT_NEAR(lambda_ran, ((3 * sleep_ms) / should_run_every_ms) + 1, 1);
    EXPECT_NEAR(seq.overruns, 0u, 1);
}

TEST_F(TestScheduler, test_sched_as_fast)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Test is buggy under valgrind debugger";
    Scheduler seq("seq");
    int lambda_ran = 0;
    std::function<bool()> fun = [&lambda_ran]() -> bool {
        ++lambda_ran;
        return true;
    };
    time_t now = seq.now();
    seq.add_task(new Task(fun, now + time::milli(5)));
    seq.add_task(new Task(fun, now + time::milli(20)));
    seq.add_task(new Task(fun, now + time::milli(30)));
    seq.add_task(new Task(fun, now + time::milli(40)));
    seq.add_task(new Task(fun, now + time::milli(50)));
    seq.set_as_fast_as_possible(true);
    seq.start();
    SIHD_LOG(debug, "Started scheduler");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    seq.stop();
    SIHD_LOG(debug, "Stopped scheduler");
    EXPECT_EQ(lambda_ran, 5);
}

TEST_F(TestScheduler, test_sched_burst)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Test is buggy under valgrind debugger";
    Scheduler seq("seq");
    int lambda_ran = 0;
    std::function<bool()> fun = [&lambda_ran]() -> bool {
        ++lambda_ran;
        return true;
    };
    // repeat task as fast as possible
    seq.add_task(new Task(fun, 0, 1));
    // spam state change
    std::thread t1([&]() {
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
    });
    // spam new tasks
    std::thread t2([&]() {
        int i = 0;
        while (i < 100)
        {
            seq.add_task(new Task(fun, seq.now() + time::us(100)));
            seq.add_task(new Task(fun, seq.now() + time::us(200)));
            seq.add_task(new Task(fun, seq.now() + time::us(300)));
            seq.add_task(new Task(fun, seq.now() + time::us(400)));
            std::this_thread::sleep_for(std::chrono::microseconds(300));
            ++i;
        }
    });
    // create and delete new tasks
    std::thread t3([&]() {
        int i = 0;
        while (i < 100)
        {
            Task *t = new Task(fun, seq.now() + time::seconds(303));
            seq.add_task(t);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            seq.remove_task(t);
            delete t;
            ++i;
        }
    });
    // thread 1 start scheduler
    t1.join();
    t2.join();
    t3.join();
    SIHD_LOG(debug, "Total executions: {}", lambda_ran);
}
} // namespace test
