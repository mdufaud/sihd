#include <cstdint>
#include <numeric>
#include <optional>
#include <thread>
#include <vector>

#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/ThreadPool.hpp>

#include <benchmark/benchmark.h>

namespace
{

using namespace sihd::util;

// no contention: the cost of the lock and the notification around a push and a pop
void BM_safequeue_push_pop(benchmark::State & state)
{
    SafeQueue<int64_t> queue;
    int64_t value = 0;
    for (auto _ : state)
    {
        queue.push(++value);
        std::optional<int64_t> popped = queue.try_pop();
        benchmark::DoNotOptimize(popped);
    }
    state.SetItemsProcessed(state.iterations());
}

// producer threads push 1..per_producer each, this thread pops them all
void BM_safequeue_producers(benchmark::State & state)
{
    constexpr int64_t per_producer = 100000;
    const int64_t producers = state.range(0);
    const int64_t expected_sum = producers * per_producer * (per_producer + 1) / 2;
    SafeQueue<int64_t> queue;
    for (auto _ : state)
    {
        std::vector<std::jthread> threads;
        for (int64_t i = 0; i < producers; ++i)
        {
            threads.emplace_back([&queue] {
                for (int64_t value = 1; value <= per_producer; ++value)
                    queue.push(value);
            });
        }

        int64_t sum = 0;
        for (int64_t popped = 0; popped < producers * per_producer; ++popped)
            sum += queue.pop_wait().value_or(0);

        if (sum != expected_sum)
        {
            state.SkipWithError("popped values do not add up to the pushed ones");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations() * producers * per_producer);
}

// one job at a time: submit, run on a pool thread, wake the waiting future
void BM_threadpool_job_round_trip(benchmark::State & state)
{
    ThreadPool pool("bench", static_cast<size_t>(state.range(0)));
    int64_t value = 0;
    for (auto _ : state)
    {
        ++value;
        if (pool.add_job([value] { return value; }).get() != value)
        {
            state.SkipWithError("the job returned another value");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations());
}

// a burst of tiny jobs spread over the pool threads, each with its own future
void BM_threadpool_burst(benchmark::State & state)
{
    constexpr size_t jobs = 1000;
    const std::vector<size_t> expected = [] {
        std::vector<size_t> indexes(jobs);
        std::iota(indexes.begin(), indexes.end(), 0);
        return indexes;
    }();
    ThreadPool pool("bench", static_cast<size_t>(state.range(0)));
    for (auto _ : state)
    {
        if (pool.complete_all_jobs(jobs, [](size_t i) { return i; }) != expected)
        {
            state.SkipWithError("jobs results out of order");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(jobs));
}

BENCHMARK(BM_safequeue_push_pop);
BENCHMARK(BM_safequeue_producers)->ArgName("producers")->Arg(1)->Arg(4)->UseRealTime()->Unit(benchmark::kMillisecond);
BENCHMARK(BM_threadpool_job_round_trip)->ArgName("threads")->Arg(1)->Arg(4)->UseRealTime();
BENCHMARK(BM_threadpool_burst)->ArgName("threads")->Arg(1)->Arg(4)->UseRealTime()->Unit(benchmark::kMicrosecond);

} // namespace
