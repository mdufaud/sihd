#include <condition_variable>
#include <mutex>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/profiling.hpp>
#include <sihd/util/time.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace std::chrono;

class TestScheduler: public ::testing::Test,
                     public IRunnable
{
    protected:
        TestScheduler() { sihd::util::LoggerManager::stream(); }

        virtual ~TestScheduler() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        bool run()
        {
            time_point<steady_clock, nanoseconds> now = _clock.now();
            time_t diff_micro = duration_cast<microseconds>(now - _last).count();
            if (_last.time_since_epoch().count() != 0)
            {
                if ((diff_micro < (this->should_run_every_us + this->delta_us)
                     && diff_micro > (this->should_run_every_us - this->delta_us))
                    == false)
                {
                    SIHD_CERR("Overrun: {} microseconds since last run (max: {} usec)\n",
                              diff_micro,
                              this->should_run_every_us + this->delta_us);
                    this->good_freq = false;
                    overruns++;
                }
            }
            _last = now;
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
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Scheduler sched("sched");

    Timestamp task_first = 0;
    Timestamp task_second = 0;
    Timestamp task_third = 0;

#if defined(__SIHD_EMSCRIPTEN__)
    // emscripten thread-wake granularity is coarse: space tasks in milliseconds
    // so their execution timestamps stay distinct instead of tying
    const auto step = std::chrono::milliseconds(10);
    const auto wait = std::chrono::milliseconds(100);
#else
    const auto step = std::chrono::microseconds(100);
    const auto wait = std::chrono::milliseconds(50);
#endif

    sched.add_task(new Task(
        [&] {
            task_second = sched.now();
            return true;
        },
        {.run_in = step * 2}));

    sched.add_task(new Task(
        [&] {
            task_third = sched.now();
            return true;
        },
        {.run_in = step * 3}));

    sched.add_task(new Task(
        [&] {
            task_first = sched.now();
            return true;
        },
        {.run_in = step}));

    sched.set_start_synchronised(true);
    sched.start();

    std::this_thread::sleep_for(wait);

    sched.stop();

    EXPECT_LT(task_first, task_second);
    EXPECT_LT(task_second, task_third);
}

TEST_F(TestScheduler, test_sched_perf)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Perf under valgrind debugger is unthinkable";

    Scheduler sched("sched");

    // most overruns are below 100 microseconds
    this->delta_us = 100;
    sched.overrun_at = time::micro(this->delta_us);

    this->should_run_every_us = 100;
    sched.add_task(new Task(this, {.reschedule_time = std::chrono::microseconds(this->should_run_every_us)}));

    sched.set_start_synchronised(true);
    sched.start();

    constexpr time_t test_multiplier = 1;
    constexpr time_t sleep_time = 50 * test_multiplier;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

    sched.stop();

    const int expected_run = time::micro(sleep_time) / this->should_run_every_us;

    auto level = num::near(this->ran, expected_run, 1) ? LogLevel::info : LogLevel::error;

    // 10% miss maximum
    const int expected_overruns = this->ran / 10;

    SIHD_LOG_INFO("Scheduler ran on time {} times, expected {}, percent success: {}%",
                  this->ran - overruns,
                  expected_run,
                  ((this->ran - overruns) / (float)expected_run) * 100);

    SIHD_LOG_LVL(level,
                 "Scheduler overruns: total={} test_calculated={} (expected less than {})",
                 sched.overruns.load(),
                 overruns,
                 (expected_run / this->ran) * 100,
                 expected_overruns);

    const int minimum_run = expected_run / 3;
    EXPECT_GT(this->ran, minimum_run);
}

TEST_F(TestScheduler, test_sched_stop)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    Scheduler sched("sched");

    std::mutex mutex;
    std::condition_variable cv;
    bool first_ran = false;
    bool second_ran = false;

    sched.add_task(new Task(
        [&]() -> bool {
            SIHD_TRACE("Should run once");
            {
                std::lock_guard lock(mutex);
                first_ran = true;
            }
            cv.notify_one();
            return true;
        },
        {.run_in = time::milli(10)}));
    sched.add_task(new Task(
        [&]() -> bool {
            SIHD_TRACE("Should not run");
            {
                std::lock_guard lock(mutex);
                second_ran = true;
            }
            cv.notify_one();
            return true;
        },
        {.run_in = time::milli(70)}));
    sched.set_start_synchronised(true);
    sched.start();

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::milliseconds(100), [&] { return first_ran; }));
    }
    sched.stop();

    {
        std::unique_lock lock(mutex);
        EXPECT_FALSE(cv.wait_for(lock, std::chrono::milliseconds(100), [&] { return second_ran; }));
    }
}

TEST_F(TestScheduler, test_sched_pause)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    constexpr time_t should_run_every_ms = 15;
    constexpr time_t sleep_ms = should_run_every_ms + 15;

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
    if (test::is_run_by_valgrind())
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
    if (test::is_run_by_valgrind())
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

// clock wrapper counting every query - exposes scheduling loop health as a plain counter
class CountingSteadyClock: public sihd::util::IClock
{
    public:
        Timestamp now() const override
        {
            calls.fetch_add(1, std::memory_order_relaxed);
            return _clock.now();
        }
        bool is_steady() const override { return true; }
        bool start() override { return true; }
        bool stop() override { return true; }

        mutable std::atomic<int> calls = 0;

    private:
        std::chrono::steady_clock _clock;
};

// clock the test controls: frozen until advanced - deadlines become exact arithmetic instead of
// wall measurements
class ManualClock: public sihd::util::IClock
{
    public:
        ManualClock(Timestamp now): _now(now.get()) {}

        Timestamp now() const override
        {
            calls.fetch_add(1, std::memory_order_relaxed);
            return Timestamp(_now.load());
        }
        bool is_steady() const override { return true; }
        bool start() override { return true; }
        bool stop() override { return true; }

        void advance(Timestamp now) { _now.store(now.get()); }

        mutable std::atomic<int> calls = 0;

    private:
        std::atomic<int64_t> _now;
};

// Classifies scheduler wakeups with a decade-wide chasm instead of latency tolerances:
// a queue change while sleeping must wake the worker (urgent plays at ~400ms), a scheduler
// sleeping until the stale far deadline cannot play it before ~900ms - cpu load moves both
// sides by milliseconds, not by the 250ms+ that separate the two classes.
TEST_F(TestScheduler, test_sched_wakeups_qualifying)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Timing classification unstable under valgrind";
    Scheduler sched("sched-wakeups");

    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<int> seq = 0;
    std::atomic<int> bulk_ran = 0;
    int urgent_seq = -1;
    int bulk_seq = -1;
    int far_seq = -1;
    bool urgent_ran = false;
    bool far_ran = false;

    // the far deadline the worker sleeps on at start
    sched.add_task(new Task(
        [&] {
            std::lock_guard lock(mutex);
            far_seq = seq.fetch_add(1);
            far_ran = true;
            cv.notify_all();
            return true;
        },
        {.run_in = time::sec(1)}));

    // a bulk of nearer one shots - only the first may wake the worker
    for (int i = 0; i < 200; ++i)
    {
        sched.add_task(new Task(
            [&] {
                ++bulk_ran;
                std::lock_guard lock(mutex);
                if (bulk_seq < 0)
                    bulk_seq = seq.fetch_add(1);
                return true;
            },
            {.run_in = time::milli(800)}));
    }

    sched.set_start_synchronised(true);
    sched.start();

    // let the worker fall asleep toward the far deadline
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto insert_tp = steady_clock::now();

    sched.add_task(new Task(
        [&] {
            std::lock_guard lock(mutex);
            urgent_seq = seq.fetch_add(1);
            urgent_ran = true;
            cv.notify_all();
            return true;
        },
        {.run_in = time::milli(400)}));

    {
        std::unique_lock lock(mutex);
        // healthy: satisfied at ~500ms - 3s ceiling for slow machines
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return urgent_ran; }));
    }
    const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - insert_tp);

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return bulk_ran == 200 && far_ran; }));
    }

    EXPECT_LT(elapsed.count(), 650);
    // ordering sanity: urgent before bulk before far
    EXPECT_GE(urgent_seq, 0);
    EXPECT_GE(bulk_seq, 0);
    EXPECT_GE(far_seq, 0);
    EXPECT_LT(urgent_seq, bulk_seq);
    EXPECT_LT(bulk_seq, far_seq);

    sched.stop();
}

// Idle gap plus emptied queue: the worker must sleep (a handful of clock queries) and survive a
// map emptied under it - a stale-deadline spin would count millions of queries and crash.
TEST_F(TestScheduler, test_sched_idle_gap_classifying)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Timing classification unstable under valgrind";
    Scheduler sched("sched-idle");
    CountingSteadyClock clock;
    sched.set_clock(&clock);

    std::mutex mutex;
    std::condition_variable cv;
    bool first_ran = false;

    sched.add_task(new Task(
        [&] {
            std::lock_guard lock(mutex);
            first_ran = true;
            cv.notify_all();
            return true;
        },
        {.run_in = time::milli(50)}));

    // far behind the first: an idle gap the worker must sleep through
    Task *second = new Task([] { return true; }, {.run_in = time::milli(300)});
    sched.add_task(second);

    sched.set_start_synchronised(true);
    sched.start();

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return first_ran; }));
    }

    // empty the queue while the worker sleeps toward the second deadline
    EXPECT_TRUE(sched.remove_task(second));
    delete second;

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    sched.stop();

    EXPECT_LT(clock.calls.load(), 10000);
}

// A task that throws must not take the process down: the poison is swallowed, the periodic
// cadence keeps ticking and later tasks still play. Pure predicate assertions.
TEST_F(TestScheduler, test_sched_exception_survives)
{
    Scheduler sched("sched-exc");

    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<int> ticks = 0;
    bool after_poison_ran = false;

    sched.add_task(new Task(
        [&] {
            ++ticks;
            cv.notify_all();
            return true;
        },
        {.reschedule_time = time::milli(15)}));
    sched.add_task(new Task([&]() -> bool { throw std::runtime_error("poison"); }, {.run_in = time::milli(1)}));
    sched.add_task(new Task(
        [&] {
            std::lock_guard lock(mutex);
            after_poison_ran = true;
            cv.notify_all();
            return true;
        },
        {.run_in = time::milli(30)}));

    sched.set_start_synchronised(true);
    sched.start();

    {
        std::unique_lock lock(mutex);
        EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] { return ticks.load() >= 3 && after_poison_ran; }));
    }
    EXPECT_GE(ticks.load(), 3);

    sched.stop();
}

// Lateness policies verified by exact grid arithmetic on a frozen clock - no wall measurement.
TEST_F(TestScheduler, test_sched_grid_arithmetic_no_clock)
{
    const Timestamp frozen_now = time::sec(100);
    const Duration grid = time::milli(5);

    // default policy (replay_missed): the grid stays where it was put - missed runs replay in a burst
    {
        Scheduler sched("sched-grid-catchup");
        ManualClock clock(frozen_now);
        sched.set_clock(&clock);

        std::mutex mutex;
        std::condition_variable cv;
        std::vector<Timestamp> played;
        Task *task = nullptr;
        task = new Task(
            [&] {
                std::lock_guard lock(mutex);
                if (played.size() < 8)
                    played.push_back(task->run_at);
                cv.notify_all();
                return true;
            },
            {.run_at = frozen_now - grid * 3, .reschedule_time = grid});
        sched.add_task(task);

        sched.set_start_synchronised(true);
        sched.start();
        {
            std::unique_lock lock(mutex);
            ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return played.size() >= 4; }));
        }
        sched.stop();

        // overdue slots replayed as-is, then the future grid
        EXPECT_EQ(played[0].get(), (frozen_now - grid * 3).get());
        EXPECT_EQ(played[1].get(), (frozen_now - grid * 2).get());
        EXPECT_EQ(played[2].get(), (frozen_now - grid * 1).get());
        EXPECT_EQ(played[3].get(), frozen_now.get());
    }

    // skip_missed policy + skip_missed_on_start: jump to the next future slot, drop missed runs
    {
        Scheduler sched("sched-grid-skip");
        ManualClock clock(frozen_now);
        sched.set_clock(&clock);
        ASSERT_TRUE(sched.set_skip_missed_on_start(true));

        std::mutex mutex;
        std::condition_variable cv;
        std::vector<Timestamp> played;
        Task *task = nullptr;
        task = new Task(
            [&] {
                std::lock_guard lock(mutex);
                if (played.size() < 8)
                    played.push_back(task->run_at);
                cv.notify_all();
                return true;
            },
            {.run_at = frozen_now - grid * 3, .reschedule_time = grid, .late_policy = LatenessPolicy::skip_missed});
        sched.add_task(task);

        sched.set_start_synchronised(true);
        sched.start();
        // start_synchronised only syncs at thread entry: the worker's first clock read captures
        // _begin_run (T), the second is the post-prepare deadline check - wait for both instead of
        // sleeping a guessed duration, so the skip jump below uses T whatever the machine load
        for (int i = 0; i < 10000 && clock.calls.load() < 2; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ASSERT_GE(clock.calls.load(), 2);
        clock.advance(frozen_now + grid);
        {
            std::unique_lock lock(mutex);
            ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return played.size() >= 1; }));
        }
        sched.stop();

        // first slot is the next grid multiple strictly after now: (T - 3g) + 4g == T + g
        EXPECT_EQ(played[0].get(), (frozen_now + grid).get());
    }
}

// sleep_then_spin must sleep most of the wait on the condition variable, then poll the clock over
// the last spin_window: a 100ms poll costs hundreds of thousands of clock reads against the handful
// a pure cv sleep spends on the whole 500ms wait - the old short-waits-only behavior would spin
// nothing at all here. The window is wide enough that coarse timers (windows, wine) cannot
// overshoot the sleep past the whole spin window.
TEST_F(TestScheduler, test_sched_spin_sleep_classification)
{
    if (test::is_run_by_valgrind())
        GTEST_SKIP() << "Timing classification unstable under valgrind";
    Scheduler sched("sched-spin");
    CountingSteadyClock clock;
    sched.set_clock(&clock);
    ASSERT_TRUE(sched.set_idle_policy(IdlePolicy::sleep_then_spin));
    ASSERT_TRUE(sched.set_spin_window(time::milli(100)));

    std::mutex mutex;
    std::condition_variable cv;
    bool ran = false;

    sched.add_task(new Task(
        [&] {
            std::lock_guard lock(mutex);
            ran = true;
            cv.notify_all();
            return true;
        },
        {.run_in = time::milli(500)}));

    sched.set_start_synchronised(true);
    sched.start();

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return ran; }));
    }
    sched.stop();

    // the last 10ms of the 50ms wait were polled
    EXPECT_GT(clock.calls.load(), 1000);
}

} // namespace test
