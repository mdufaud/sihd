#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

#include <sihd/util/ALogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerAsync.hpp>
#include <sihd/util/LoggerFilter.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/LoggerStream.hpp>

#include <benchmark/benchmark.h>

using namespace sihd::util;

SIHD_NEW_LOGGER("bench");

namespace
{

class CountSink: public ALogger
{
    public:
        CountSink(std::atomic<int64_t> & count): ALogger("count"), _count(count) {}

        void log([[maybe_unused]] const LogInfo & info, [[maybe_unused]] std::string_view msg) override
        {
            _count.fetch_add(1, std::memory_order_relaxed);
        }

    private:
        std::atomic<int64_t> & _count;
};

enum class Sink
{
    count,
    level_filtered,
    regex_filtered,
    stream,
    async,
};

// past any run: a full queue drops, and a drop costs less than a log
constexpr size_t async_queue_size = 1 << 22;
constexpr int64_t async_iterations = 200000;

void reset_loggers()
{
    LoggerManager::set_level(LogLevel::debug);
    LoggerManager::clear_loggers();
    LoggerManager::clear_filters();
}

void install(Sink sink, std::atomic<int64_t> & delivered, std::FILE *dev_null)
{
    switch (sink)
    {
        case Sink::count:
            LoggerManager::add(new CountSink(delivered));
            break;
        case Sink::level_filtered:
            LoggerManager::add(new CountSink(delivered));
            LoggerManager::set_level(LogLevel::notice);
            break;
        case Sink::regex_filtered:
            LoggerManager::add(new CountSink(delivered));
            LoggerManager::filter(new LoggerFilter({.source_regex = "^bench$"}));
            break;
        case Sink::stream:
            LoggerManager::add(new LoggerStream(dev_null));
            break;
        case Sink::async:
            LoggerManager::add(new LoggerAsync(new CountSink(delivered), async_queue_size));
            break;
    }
}

// sinks are global: thread 0 installs and checks them, the loop barriers hold the other threads
void BM_log(benchmark::State & state, Sink sink, int64_t delivered_per_log)
{
    std::atomic<int64_t> delivered {0};
    std::FILE *dev_null = nullptr;
    if (state.thread_index() == 0)
    {
        dev_null = std::fopen("/dev/null", "w");
        reset_loggers();
        install(sink, delivered, dev_null);
    }

    for (auto _ : state)
        SIHD_LOG(debug, "bench {} payload {}", 42, "0123456789");

    if (state.thread_index() == 0)
    {
        // drains the async queue
        reset_loggers();
        std::fclose(dev_null);
        const int64_t expected = state.iterations() * state.threads() * delivered_per_log;
        if (sink != Sink::stream && delivered.load() != expected)
            state.SkipWithError("sink got " + std::to_string(delivered.load()) + " logs, expected "
                                + std::to_string(expected));
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_CAPTURE(BM_log, count_sink, Sink::count, 1)->Threads(1)->Threads(4)->UseRealTime();
BENCHMARK_CAPTURE(BM_log, level_filtered, Sink::level_filtered, 0)->Threads(1)->Threads(4)->UseRealTime();
BENCHMARK_CAPTURE(BM_log, regex_filtered, Sink::regex_filtered, 0)->Threads(1)->Threads(4)->UseRealTime();
BENCHMARK_CAPTURE(BM_log, stream_sink, Sink::stream, 1)->Threads(1)->Threads(4)->UseRealTime();
// fixed iterations: the queue never fills, the drain runs alongside
BENCHMARK_CAPTURE(BM_log, async_sink, Sink::async, 1)
    ->Iterations(async_iterations)
    ->Threads(1)
    ->Threads(4)
    ->UseRealTime();

} // namespace
