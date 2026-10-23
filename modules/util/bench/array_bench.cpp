#include <cstdint>
#include <vector>

#include <sihd/util/Array.hpp>

#include <benchmark/benchmark.h>

namespace
{

using namespace sihd::util;

struct Item
{
        int key;
        int value;
};

constexpr int64_t item_count = 30000;

template <typename Container>
Container make_filled(int64_t count)
{
    Container container;
    for (int i = 0; i < count; ++i)
        container.push_back({i, i + 1});
    return container;
}

template <typename Container>
void BM_push_back(benchmark::State & state)
{
    for (auto _ : state)
    {
        Container container = make_filled<Container>(state.range(0));
        benchmark::DoNotOptimize(container.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <typename Container>
void BM_index_read(benchmark::State & state)
{
    const Container container = make_filled<Container>(state.range(0));
    for (auto _ : state)
    {
        int64_t sum = 0;
        for (size_t i = 0; i < container.size(); ++i)
            sum += container[i].key;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <typename Container>
void BM_iterate(benchmark::State & state)
{
    // Array has no const begin()/end()
    Container container = make_filled<Container>(state.range(0));
    for (auto _ : state)
    {
        int64_t sum = 0;
        for (const Item & item : container)
            sum += item.value;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <typename Container>
void BM_copy(benchmark::State & state)
{
    const Container container = make_filled<Container>(state.range(0));
    for (auto _ : state)
    {
        Container copy = container;
        benchmark::DoNotOptimize(copy.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <typename Container>
void BM_pop_back(benchmark::State & state)
{
    const Container filled = make_filled<Container>(state.range(0));
    for (auto _ : state)
    {
        state.PauseTiming();
        Container container = filled;
        state.ResumeTiming();
        while (container.size() > 0)
        {
            benchmark::DoNotOptimize(container.back());
            container.pop_back();
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK_TEMPLATE(BM_push_back, std::vector<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_push_back, Array<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_index_read, std::vector<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_index_read, Array<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_iterate, std::vector<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_iterate, Array<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_copy, std::vector<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_copy, Array<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_pop_back, std::vector<Item>)->Arg(item_count);
BENCHMARK_TEMPLATE(BM_pop_back, Array<Item>)->Arg(item_count);

} // namespace
