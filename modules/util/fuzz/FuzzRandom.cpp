#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <sihd/util/Random.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const uint64_t seed = provider.ConsumeIntegral<uint64_t>();
    const uint64_t first = provider.ConsumeIntegral<uint64_t>();
    const uint64_t second = provider.ConsumeIntegral<uint64_t>();
    const double dfirst = provider.ConsumeFloatingPointInRange<double>(-1e9, 1e9);
    const double dsecond = provider.ConsumeFloatingPointInRange<double>(-1e9, 1e9);
    const float ffirst = provider.ConsumeFloatingPointInRange<float>(-1e6f, 1e6f);
    const float fsecond = provider.ConsumeFloatingPointInRange<float>(-1e6f, 1e6f);

    Random left(seed);
    Random right(seed);
    for (int i = 0; i < 8; ++i)
    {
        const uint64_t value = left.rand(first, second);
        test::fuzz_check(value == right.rand(first, second), "a seed gives one sequence");
        test::fuzz_check(value >= std::min(first, second) && value <= std::max(first, second),
                         "rand stays within swapped bounds");

        const double dvalue = left.drand(dfirst, dsecond);
        test::fuzz_check(dvalue == right.drand(dfirst, dsecond), "a seed gives one drand sequence");
        test::fuzz_check(dvalue >= std::min(dfirst, dsecond) && dvalue <= std::max(dfirst, dsecond),
                         "drand stays within swapped bounds");

        const float fvalue = left.frand(ffirst, fsecond);
        test::fuzz_check(fvalue == right.frand(ffirst, fsecond), "a seed gives one frand sequence");
        test::fuzz_check(fvalue >= std::min(ffirst, fsecond) && fvalue <= std::max(ffirst, fsecond),
                         "frand stays within swapped bounds");
    }
    test::fuzz_check(Random(seed).rand(first, first) == first, "rand of an empty interval is that value");
    return 0;
}
