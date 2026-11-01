#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>

#include <sihd/core/Channel.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Slice.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::util;
using sihd::core::Channel;

Slice consume_slice(FuzzedDataProvider & provider)
{
    return {provider.ConsumeIntegralInRange<ssize_t>(-100, 100), provider.ConsumeIntegralInRange<ssize_t>(-100, 100)};
}

void fuzz_copy_to(FuzzedDataProvider & provider)
{
    const size_t count = provider.ConsumeIntegralInRange<size_t>(1, 64);
    ArrInt src;
    src.resize(count);
    for (size_t i = 0; i < count; ++i)
        src[i] = provider.ConsumeIntegral<int>();
    Channel channel("chan", "int", count);
    test::fuzz_check(channel.write(src), "a channel takes an array of its size");

    const Slice slice = consume_slice(provider);
    const Slice::Range range = slice.resolve(count);
    ArrInt dst;
    dst.resize(range.size());
    test::fuzz_check(channel.copy_to(dst, slice) == !range.empty(), "copy_to fails exactly on an empty slice");
    for (size_t i = 0; i < range.size(); ++i)
        test::fuzz_check(dst[i] == src[range.from + i], "copy_to copies the resolved elements");
}

void fuzz_copy_to_bytes(FuzzedDataProvider & provider)
{
    const size_t count = provider.ConsumeIntegralInRange<size_t>(1, 64);
    Array<uint8_t> src;
    src.resize(count);
    for (size_t i = 0; i < count; ++i)
        src[i] = provider.ConsumeIntegral<uint8_t>();
    Channel channel("chan", "ubyte", count);
    test::fuzz_check(channel.write(src), "a channel takes an array of its size");

    const Slice slice = consume_slice(provider);
    const Slice::Range range = slice.resolve(count);
    Array<uint8_t> dst;
    dst.resize(range.size());
    test::fuzz_check(channel.copy_to_bytes(dst, slice) == !range.empty(),
                     "copy_to_bytes fails exactly on an empty slice");
    for (size_t i = 0; i < range.size(); ++i)
        test::fuzz_check(dst[i] == src[range.from + i], "copy_to_bytes copies the resolved bytes");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_copy_to(provider);
    else
        fuzz_copy_to_bytes(provider);
    return 0;
}
