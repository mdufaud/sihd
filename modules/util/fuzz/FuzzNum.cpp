#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include <sihd/util/num.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::util;

template <typename T>
void fuzz_saturation(FuzzedDataProvider & provider)
{
    const T a = provider.ConsumeIntegral<T>();
    const T b = provider.ConsumeIntegral<T>();
    // wider than every T: the exact result of the operation always fits
    using Wide = __int128;
    const Wide wide_a = a;
    const Wide wide_b = b;
    const Wide low = std::numeric_limits<T>::min();
    const Wide high = std::numeric_limits<T>::max();

    test::fuzz_check(Wide(num::add_no_overflow(a, b)) == std::clamp(wide_a + wide_b, low, high),
                     "add saturates instead of wrapping");
    test::fuzz_check(Wide(num::substract_no_overflow(a, b)) == std::clamp(wide_a - wide_b, low, high),
                     "substract saturates instead of wrapping");
}

void fuzz_near(FuzzedDataProvider & provider)
{
    const int32_t value = provider.ConsumeIntegral<int32_t>();
    const int32_t expected = provider.ConsumeIntegral<int32_t>();
    const int32_t error = provider.ConsumeIntegralInRange<int32_t>(0, std::numeric_limits<int32_t>::max());
    const int64_t diff = static_cast<int64_t>(value) - static_cast<int64_t>(expected);
    test::fuzz_check(num::near<int64_t>(value, expected, error) == (std::abs(diff) <= error),
                     "near compares the distance");
}

void fuzz_rand(FuzzedDataProvider & provider)
{
    const uint64_t first = provider.ConsumeIntegral<uint64_t>();
    const uint64_t second = provider.ConsumeIntegral<uint64_t>();
    const uint64_t value = num::rand(first, second);
    test::fuzz_check(value >= std::min(first, second) && value <= std::max(first, second),
                     "rand stays within swapped bounds");
    test::fuzz_check(num::rand(first, first) == first, "rand of an empty interval is that value");

    const double dfirst = provider.ConsumeFloatingPointInRange<double>(-1e9, 1e9);
    const double dsecond = provider.ConsumeFloatingPointInRange<double>(-1e9, 1e9);
    const double dvalue = num::drand(dfirst, dsecond);
    test::fuzz_check(dvalue >= std::min(dfirst, dsecond) && dvalue <= std::max(dfirst, dsecond),
                     "drand stays within swapped bounds");

    const float ffirst = provider.ConsumeFloatingPointInRange<float>(-1e6f, 1e6f);
    const float fsecond = provider.ConsumeFloatingPointInRange<float>(-1e6f, 1e6f);
    const float fvalue = num::frand(ffirst, fsecond);
    test::fuzz_check(fvalue >= std::min(ffirst, fsecond) && fvalue <= std::max(ffirst, fsecond),
                     "frand stays within swapped bounds");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 5))
    {
        case 0:
            fuzz_saturation<int32_t>(provider);
            break;
        case 1:
            fuzz_saturation<uint32_t>(provider);
            break;
        case 2:
            fuzz_saturation<int64_t>(provider);
            break;
        case 3:
            fuzz_saturation<uint64_t>(provider);
            break;
        case 4:
            fuzz_near(provider);
            break;
        default:
            fuzz_rand(provider);
            break;
    }
    return 0;
}
