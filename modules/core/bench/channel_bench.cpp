#include <chrono>
#include <cstdint>
#include <list>
#include <string>
#include <thread>

#include <sihd/core/Channel.hpp>
#include <sihd/core/ChannelWaiter.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/IHandler.hpp>

#include <benchmark/benchmark.h>

namespace
{

using namespace sihd::core;
using namespace sihd::util;

class CountObserver: public IHandler<Channel *>
{
    public:
        void handle([[maybe_unused]] Channel *channel) override { ++count; }

        int64_t count = 0;
};

// observers run in the writer thread: a write is the copy, the timestamp and every observer
void BM_channel_write_scalar(benchmark::State & state)
{
    Channel channel("bench", TYPE_INT, 1);
    std::list<CountObserver> observers(static_cast<size_t>(state.range(0)));
    for (CountObserver & observer : observers)
        channel.add_observer(&observer);

    // write_on_change is on by default: a changing value notifies every write
    int32_t value = 0;
    for (auto _ : state)
    {
        if (!channel.write(0, ++value))
        {
            state.SkipWithError("write refused");
            break;
        }
    }

    for (const CountObserver & observer : observers)
    {
        if (observer.count != state.iterations())
            state.SkipWithError("an observer missed notifications");
    }
    state.SetItemsProcessed(state.iterations());
}

void BM_channel_write_array(benchmark::State & state)
{
    const size_t size = static_cast<size_t>(state.range(0));
    Channel channel("bench", TYPE_DOUBLE, size);
    CountObserver observer;
    channel.add_observer(&observer);
    ArrDouble samples;
    samples.resize(size);

    for (auto _ : state)
    {
        samples[0] += 1.0;
        if (!channel.write(samples))
        {
            state.SkipWithError("write refused");
            break;
        }
    }

    if (observer.count != state.iterations())
        state.SkipWithError("the observer missed notifications");
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(size * sizeof(double)));
}

// write_on_change drops a write of the current value: compare only, no copy nor notification
void BM_channel_write_unchanged(benchmark::State & state)
{
    const size_t size = static_cast<size_t>(state.range(0));
    Channel channel("bench", TYPE_DOUBLE, size);
    CountObserver observer;
    channel.add_observer(&observer);
    ArrDouble samples;
    samples.resize(size);
    channel.write(samples);
    observer.count = 0;

    for (auto _ : state)
    {
        if (!channel.write(samples))
        {
            state.SkipWithError("write refused");
            break;
        }
    }

    if (observer.count != 0)
        state.SkipWithError("an unchanged write notified");
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(size * sizeof(double)));
}

// a write wakes a thread waiting on the channel, which answers on another: two cross-thread wakeups
void BM_channel_ping_pong(benchmark::State & state)
{
    constexpr int32_t stop_value = -1;
    const Duration timeout = Duration(std::chrono::seconds(5));
    Channel ping("ping", TYPE_INT, 1);
    Channel pong("pong", TYPE_INT, 1);
    ChannelWaiter ping_waiter(&ping);
    ChannelWaiter pong_waiter(&pong);

    std::jthread responder([&] {
        for (uint32_t round = 1;; ++round)
        {
            if (!ping_waiter.wait_for_nb(timeout, round))
                return;
            const int32_t value = ping.read<int32_t>(0);
            if (value == stop_value)
                return;
            pong.write(0, value);
        }
    });

    // counts are absolute: a wait never misses a notification that came before it
    int32_t round = 0;
    for (auto _ : state)
    {
        ping.write(0, ++round);
        if (!pong_waiter.wait_for_nb(timeout, static_cast<uint32_t>(round)))
        {
            state.SkipWithError("no pong");
            break;
        }
    }
    ping.write(0, stop_value);
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_channel_write_scalar)->ArgName("observers")->Arg(0)->Arg(1)->Arg(8);
BENCHMARK(BM_channel_write_array)->ArgName("doubles")->Arg(16)->Arg(4096);
BENCHMARK(BM_channel_write_unchanged)->ArgName("doubles")->Arg(16)->Arg(4096);
BENCHMARK(BM_channel_ping_pong)->UseRealTime();

} // namespace
