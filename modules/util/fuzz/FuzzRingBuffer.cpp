#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include <sihd/util/RingBuffer.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const size_t capacity = provider.ConsumeIntegralInRange<size_t>(0, 16);
    RingBuffer<int> ring(capacity);
    std::deque<int> model;

    const size_t ops = provider.ConsumeIntegralInRange<size_t>(0, 64);
    for (size_t i = 0; i < ops; ++i)
    {
        const size_t count = provider.ConsumeIntegralInRange<size_t>(0, 20);
        switch (provider.ConsumeIntegralInRange<int>(0, 3))
        {
            case 0:
            {
                std::vector<int> in(count);
                for (int & value : in)
                    value = provider.ConsumeIntegral<int>();
                const size_t expected = std::min(count, capacity - model.size());
                test::fuzz_check(ring.write(in.data(), in.size()) == expected, "write is bounded by the free space");
                model.insert(model.end(), in.begin(), in.begin() + static_cast<ptrdiff_t>(expected));
                break;
            }
            case 1:
            case 2:
            {
                const bool consume = provider.ConsumeBool();
                std::vector<int> out(count);
                const size_t expected = std::min(count, model.size());
                const size_t got = consume ? ring.read(out.data(), out.size()) : ring.peek(out.data(), out.size());
                test::fuzz_check(got == expected, "read and peek are bounded by the content");
                test::fuzz_check(std::equal(out.begin(), out.begin() + static_cast<ptrdiff_t>(got), model.begin()),
                                 "read and peek return the oldest values first");
                if (consume)
                    model.erase(model.begin(), model.begin() + static_cast<ptrdiff_t>(got));
                break;
            }
            default:
            {
                const size_t expected = std::min(count, model.size());
                test::fuzz_check(ring.skip(count) == expected, "skip is bounded by the content");
                model.erase(model.begin(), model.begin() + static_cast<ptrdiff_t>(expected));
                break;
            }
        }
        test::fuzz_check(ring.size() == model.size(), "size follows the model");
        test::fuzz_check(ring.available() == capacity - model.size(), "available follows the model");
        test::fuzz_check(ring.empty() == model.empty() && ring.full() == (model.size() == capacity),
                         "empty and full follow the model");
    }
    return 0;
}
