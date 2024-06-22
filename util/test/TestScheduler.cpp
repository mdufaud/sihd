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

TEST_F(TestScheduler, test_sched_order)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Scheduler sched("sched");

    Timestamp task_first = 0;
    Timestamp task_second = 0;
    Timestamp task_third = 0;

    sched.add_task(new Task(
        [&] {
            task_second = sched.now();
            return true;
        },
        {.run_in = std::chrono::microseconds(200)}));

    sched.add_task(new Task(
        [&] {
            task_third = sched.now();
            return true;
        },
        {.run_in = std::chrono::microseconds(300)}));

    sched.add_task(new Task(
        [&] {
            task_first = sched.now();
            return true;
        },
        {.run_in = std::chrono::microseconds(100)}));

    sched.set_start_synchronised(true);
    sched.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    sched.stop();

    EXPECT_LT(task_first, task_second);
    EXPECT_LT(task_second, task_third);
}

TEST_F(TestScheduler, test_sched_perf)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Perf under valgrind debugger is unthinkable";

    Scheduler sched("sched");

    // most overruns are below 100 microseconds
    this->delta_us = 100;
    sched.overrun_at = time::micro(this->delta_us);

    this->should_run_every_us = 100;
    sched.add_task(new Task(this, {.reschedule_time = std::chrono::microseconds(this->should_run_every_us)}));

    sched.set_start_synchronised(true);
    sched.start();

    constexpr time_t sleep_time = 50;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

    sched.stop();

    const int expected_run = time::micro(sleep_time) / this->should_run_every_us;
    const int minimum_run = expected_run / 2;

    auto level = num::near(this->ran, expected_run, 1) ? LogLevel::info : LogLevel::error;

    SIHD_LOG_LVL(level, "Scheduler tasks executed: total={} expected={}", this->ran, expected_run);

    // 10% miss maximum
    const int expected_overruns = this->ran / 10;
    SIHD_LOG_LVL(level,
                 "Scheduler overruns: total={} test_calculated={} (expected less than {})",
                 sched.overruns,
                 overruns,
                 expected_overruns);

    EXPECT_LE(sched.overruns, minimum_run);
    EXPECT_GT(this->ran, minimum_run);
}

TEST_F(TestScheduler, test_sched_stop)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Scheduler sched("sched");

    int ran = 0;
    sched.add_task(new Task(
        [&]() -> bool {
            SIHD_TRACE("Should run once");
            ++ran;
            return true;
        },
        {.run_in = time::milli(5)}));
    sched.add_task(new Task(
        [&]() -> bool {
            SIHD_TRACE("Should not run");
            ++ran;
            return true;
        },
        {.run_in = time::milli(20)}));
    sched.set_start_synchronised(true);
    sched.start();
    SIHD_TRACE("Before sleep");
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    EXPECT_EQ(ran, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(ran, 1);
    sched.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(ran, 1);
}

TEST_F(TestScheduler, test_sched_pause)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    constexpr time_t should_run_every_ms = 15;
    constexpr time_t sleep_ms = should_run_every_ms + 10;

    Scheduler sched("sched");

    int lambda_ran = 0;
    sched.add_task(new Task(
        [&lambda_ran]() -> bool {
            ++lambda_ran;
            return true;
        },
        {.reschedule_time = time::ms(should_run_every_ms)}));
    sched.set_start_synchronised(true);
    sched.start();
    SIHD_LOG(debug, "Started scheduler");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    SIHD_LOG(debug, "Pausing scheduler");
    sched.pause();
    SIHD_LOG(debug, "Paused scheduler");
    EXPECT_GE(lambda_ran, (sleep_ms / should_run_every_ms));
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    EXPECT_GE(lambda_ran, (sleep_ms / should_run_every_ms));
    SIHD_LOG(debug, "Resuming scheduler");
    sched.resume();
    SIHD_LOG(debug, "Resumed scheduler");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    SIHD_LOG(debug, "Pausing scheduler");
    sched.pause();
    SIHD_LOG(debug, "Paused scheduler");
    EXPECT_GE(lambda_ran, ((2 * sleep_ms) / should_run_every_ms));
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    EXPECT_GE(lambda_ran, ((2 * sleep_ms) / should_run_every_ms));
    SIHD_LOG(debug, "Resuming scheduler");
    sched.resume();
    SIHD_LOG(debug, "Resumed scheduler");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

    SIHD_LOG(debug, "Stopping scheduler");
    sched.stop();
    SIHD_LOG(debug, "Stopped scheduler");
    EXPECT_GE(lambda_ran, ((3 * sleep_ms) / should_run_every_ms));
}

TEST_F(TestScheduler, test_sched_as_fast)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Scheduler sched("sched");
    int lambda_ran = 0;
    std::function<bool()> fun = [&lambda_ran]() -> bool {
        ++lambda_ran;
        return true;
    };
    sched.add_task(new Task(fun, {.run_in = time::milli(5)}));
    sched.add_task(new Task(fun, {.run_in = time::milli(20)}));
    sched.add_task(new Task(fun, {.run_in = time::milli(30)}));
    sched.add_task(new Task(fun, {.run_in = time::milli(40)}));
    sched.add_task(new Task(fun, {.run_in = time::milli(50)}));
    sched.set_no_delay(true);
    sched.set_start_synchronised(true);
    sched.start();
    SIHD_LOG(debug, "Started scheduler");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sched.stop();
    SIHD_LOG(debug, "Stopped scheduler");
    EXPECT_EQ(lambda_ran, 5);
}

TEST_F(TestScheduler, test_sched_burst)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Scheduler sched("sched");
    sched.set_start_synchronised(true);
    std::atomic<int> lambda_ran = 0;
    std::function<bool()> fun = [&lambda_ran]() -> bool {
        ++lambda_ran;
        return true;
    };
    // repeat task as fast as possible
    sched.add_task(new Task(fun, {.reschedule_time = 1}));
    // spam state change
    std::thread t1([&]() {
        int i = 0;
        while (i < 100)
        {
            sched.start();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            sched.pause();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            sched.resume();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            sched.stop();
            ++i;
        }
    });
    // spam new tasks
    std::thread t2([&]() {
        int i = 0;
        while (i < 100)
        {
            sched.add_task(new Task(fun, {.run_in = time::us(100)}));
            sched.add_task(new Task(fun, {.run_in = time::us(200)}));
            sched.add_task(new Task(fun, {.run_in = time::us(300)}));
            sched.add_task(new Task(fun, {.run_in = time::us(400)}));
            std::this_thread::sleep_for(std::chrono::microseconds(300));
            ++i;
        }
    });
    // create and delete new tasks
    std::thread t3([&]() {
        int i = 0;
        while (i < 100)
        {
            Task *t = new Task(
                [] {
                    SIHD_LOG_ERROR("Should not be played ever");
                    return false;
                },
                {.run_in = time::seconds(303)});
            sched.add_task(t);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            ASSERT_TRUE(sched.remove_task(t));
            delete t;
            ++i;
        }
    });
    // thread 1 start scheduler
    t1.join();
    t2.join();
    t3.join();
    SIHD_LOG(debug, "Total executions: {}", lambda_ran.load());
}

} // namespace test
