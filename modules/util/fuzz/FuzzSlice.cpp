#include <cstddef>
#include <cstdint>

#include <sihd/util/Slice.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const size_t length = provider.ConsumeIntegralInRange<size_t>(0, 1 << 20);
    const Slice slice {provider.ConsumeIntegral<ssize_t>(), provider.ConsumeIntegral<ssize_t>()};
    const Slice::Range range = slice.resolve(length);
    test::fuzz_check(range.from <= range.to, "resolve keeps from <= to");
    test::fuzz_check(range.to <= length, "resolve never exceeds the size");

    const ssize_t signed_length = static_cast<ssize_t>(length);
    if (slice.from >= 0 && slice.from <= slice.to && slice.to < signed_length)
    {
        test::fuzz_check(range.from == static_cast<size_t>(slice.from), "in range from is kept");
        test::fuzz_check(range.to == static_cast<size_t>(slice.to) + 1, "in range to is inclusive");
    }
    if (slice.from >= -signed_length && slice.from < 0 && slice.to >= -signed_length && slice.to < 0)
    {
        const Slice::Range positive = Slice {slice.from + signed_length, slice.to + signed_length}.resolve(length);
        test::fuzz_check(range.from == positive.from && range.to == positive.to, "negative indexes count from the end");
    }

    const size_t offset = provider.ConsumeIntegralInRange<size_t>(0, length);
    const size_t count = provider.ConsumeIntegralInRange<size_t>(0, length);
    const Slice::Range sized = Slice::from_size(offset, count).resolve(length);
    if (count == 0)
        test::fuzz_check(sized.empty(), "from_size of nothing is empty");
    else if (offset + count <= length)
        test::fuzz_check(sized.from == offset && sized.to == offset + count, "from_size covers count elements");
    return 0;
}
