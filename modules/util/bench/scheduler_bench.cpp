#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <vector>

#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/time.hpp>

#include <benchmark/benchmark.h>

using namespace sihd::util;

namespace
{

constexpr std::chrono::seconds done_timeout(10);

// no_delay: the worker never sleeps, a tick is picking, running and rescheduling one of task_count tasks
void BM_scheduler_dispatch(benchmark::State & state)
{
    constexpr int64_t ticks = 100000;
    const int64_t task_count = state.range(0);
    int64_t total_ticks = 0;
    for (auto _ : state)
    {
        Scheduler scheduler("bench");
        scheduler.set_no_delay(true);

        std::atomic<int64_t> ran {0};
        std::promise<void> done;
        std::future<void> done_future = done.get_future();
        for (int64_t i = 0; i < task_count; ++i)
        {
            scheduler.add_task(new Task(
                [&ran, &done] {
                    if (ran.fetch_add(1, std::memory_order_relaxed) + 1 == ticks)
                        done.set_value();
                    return true;
                },
                {.run_in = Duration(i + 1), .reschedule_time = Duration(task_count)}));
        }

        const auto begin = std::chrono::steady_clock::now();
        const bool started = scheduler.start();
        const bool finished = started && done_future.wait_for(done_timeout) == std::future_status::ready;
        scheduler.stop();
        state.SetIterationTime(std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count());

        if (!finished)
        {
            state.SkipWithError("scheduler did not run all the ticks");
            break;
        }
        total_ticks += ran.load();
    }
    state.SetItemsProcessed(total_ticks);
}

// one periodic task on the wall clock: the time column is the mean delay of a run past its planned time
void BM_scheduler_lateness(benchmark::State & state, IdlePolicy policy)
{
    constexpr size_t ticks = 500;
    const Duration period = Duration(time::milli(1));
    for (auto _ : state)
    {
        Scheduler scheduler("bench");
        scheduler.set_idle_policy(policy);

        std::vector<Duration> lateness;
        lateness.reserve(ticks);
        std::promise<void> done;
        std::future<void> done_future = done.get_future();

        Task *task = new Task({.run_in = period, .reschedule_time = period});
        task->set_method([&scheduler, &lateness, &done, task] {
            // run_at is the planned time until the run returns
            if (lateness.size() < ticks)
            {
                lateness.push_back(scheduler.now() - task->run_at);
                if (lateness.size() == ticks)
                    done.set_value();
            }
            return true;
        });
        scheduler.add_task(task);

        const bool started = scheduler.start();
        const bool finished = started && done_future.wait_for(done_timeout) == std::future_status::ready;
        scheduler.stop();
        if (!finished)
        {
            state.SkipWithError("scheduler did not run all the ticks");
            break;
        }

        PSquareStat<long long> stat;
        for (Duration late : lateness)
            stat.add_sample(late.nanoseconds());
        state.SetIterationTime(static_cast<double>(stat.average()) / 1e9);
        state.counters["p99_us"] = static_cast<double>(stat.p99()) / 1e3;
        state.counters["max_us"] = static_cast<double>(stat.max) / 1e3;
        state.counters["overruns"] = static_cast<double>(scheduler.overruns.load());
    }
}

BENCHMARK(BM_scheduler_dispatch)
    ->ArgName("tasks")
    ->Arg(1)
    ->Arg(64)
    ->Arg(4096)
    ->UseManualTime()
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(BM_scheduler_lateness, sleep, IdlePolicy::sleep)
    ->Iterations(1)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BM_scheduler_lateness, sleep_then_spin, IdlePolicy::sleep_then_spin)
    ->Iterations(1)
    ->UseManualTime()
    ->Unit(benchmark::kMicrosecond);

} // namespace
