#include <string>

#include <sihd/sys/Process.hpp>

#include <benchmark/benchmark.h>

namespace
{

using namespace sihd::sys;

enum class Spawn
{
    // posix_spawnp
    spawn,
    fork,
    function,
};

// launch to reaped exit: the time the caller waits for a child that does nothing
void BM_process_run(benchmark::State & state, Spawn mode)
{
    for (auto _ : state)
    {
        Process proc = mode == Spawn::function ? Process([] { return 0; }) : Process {"true"};
        if (mode == Spawn::fork)
            proc.set_force_fork(true);

        const bool ran = proc.execute().has_value() && proc.wait_exit().value_or(false);
        if (!ran || proc.return_code() != 0)
        {
            state.SkipWithError("the child did not exit 0");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_process_capture(benchmark::State & state)
{
    const std::string expected = "hello world\n";
    std::string output;
    for (auto _ : state)
    {
        output.clear();
        Process proc {"echo", "hello world"};
        proc.stdout_to(output);

        const bool ran = proc.execute().has_value() && proc.wait_any().value_or(false) && proc.terminate();
        if (!ran || output != expected)
        {
            state.SkipWithError("captured '" + output + "'");
            break;
        }
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_CAPTURE(BM_process_run, spawn, Spawn::spawn)->UseRealTime()->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BM_process_run, fork, Spawn::fork)->UseRealTime()->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BM_process_run, function, Spawn::function)->UseRealTime()->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_process_capture)->UseRealTime()->Unit(benchmark::kMicrosecond);

} // namespace
