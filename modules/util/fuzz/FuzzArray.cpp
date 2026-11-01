#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Slice.hpp>
#include <sihd/util/array_utils.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr size_t max_elements = 128;
constexpr size_t max_operations = 8;
constexpr uint8_t untouched = 0xa5;

// the array borrows an exact size heap buffer: any access past it is reported by the sanitizer
template <typename T>
void borrow(Array<T> & array, std::vector<uint8_t> & backing)
{
    if (!backing.empty())
        test::fuzz_check(array.assign_bytes(backing.data(), backing.size()), "an array borrows a buffer");
}

void fuzz_copy_bytes(FuzzedDataProvider & provider)
{
    const size_t size = provider.ConsumeIntegralInRange<size_t>(0, max_elements);
    std::vector<uint8_t> backing = test::consume_exact_bytes(provider, size);
    std::vector<uint8_t> shadow = backing;
    Array<uint8_t> array;
    borrow(array, backing);

    for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, max_operations); i > 0; --i)
    {
        const size_t offset = provider.ConsumeIntegralInRange<size_t>(0, size + 8);
        const size_t count = provider.ConsumeIntegralInRange<size_t>(0, size + 8);
        if (provider.ConsumeBool())
        {
            const std::vector<uint8_t> source = test::consume_exact_bytes(provider, count);
            const bool copied = array.copy_from_bytes(source.data(), source.size(), Slice::from_size(offset, count));
            test::fuzz_check(copied == (count > 0 && offset + count <= size),
                             "copy_from_bytes succeeds exactly when the bytes fit");
            if (copied)
                std::copy(source.begin(), source.end(), shadow.begin() + static_cast<ssize_t>(offset));
        }
        else
        {
            std::vector<uint8_t> destination(count, untouched);
            const bool copied = array.copy_to_bytes(destination.data(), Slice::from_size(offset, count));
            test::fuzz_check(copied == (count > 0 && offset < size), "copy_to_bytes succeeds exactly inside the array");
            if (count > 0 && offset + count <= size)
                test::fuzz_check(std::equal(destination.begin(), destination.end(), shadow.begin() + offset),
                                 "copy_to_bytes copies the requested bytes");
        }
        test::fuzz_check(backing == shadow, "only copy_from_bytes changes the array");
    }
}

void fuzz_slice_consistency(FuzzedDataProvider & provider)
{
    const size_t size = provider.ConsumeIntegralInRange<size_t>(0, max_elements);
    std::vector<uint8_t> backing = test::consume_exact_bytes(provider, size);
    Array<uint8_t> array;
    borrow(array, backing);
    const ArrayView<uint8_t> view(backing.data(), backing.size());
    const Slice slice {provider.ConsumeIntegral<ssize_t>(), provider.ConsumeIntegral<ssize_t>()};

    std::vector<uint8_t> from_array(size, untouched);
    std::vector<uint8_t> from_view(size, untouched);
    const bool array_copied = array.copy_to_bytes(from_array.data(), slice);
    const bool view_copied = view.copy_to_bytes(from_view.data(), slice);
    test::fuzz_check(array_copied == view_copied, "an array and its view accept the same slices");
    test::fuzz_check(from_array == from_view, "an array and its view copy the same bytes");

    const ArrayView<uint8_t> sub = view.subview(slice);
    test::fuzz_check(array_copied == !sub.empty(), "a slice is copied exactly when its subview is not empty");
    if (!array_copied)
        return;
    test::fuzz_check(sub.size() <= size, "a subview stays inside the array");
    test::fuzz_check(std::equal(sub.data(), sub.data() + sub.size(), from_array.begin()), "a subview holds the copy");
    test::fuzz_check(std::all_of(from_array.begin() + static_cast<ssize_t>(sub.size()),
                                 from_array.end(),
                                 [](uint8_t byte) { return byte == untouched; }),
                     "a copy writes nothing past its slice");
}

template <typename T>
void fuzz_subview(FuzzedDataProvider & provider)
{
    const size_t size = provider.ConsumeIntegralInRange<size_t>(0, max_elements);
    const std::vector<uint8_t> bytes = test::consume_exact_bytes(provider, size * sizeof(T));
    std::vector<T> elements(size);
    if (size > 0)
        std::memcpy(elements.data(), bytes.data(), bytes.size());
    const ArrayView<T> view(elements.data(), elements.size());

    const Slice slice {provider.ConsumeIntegral<ssize_t>(), provider.ConsumeIntegral<ssize_t>()};
    const ArrayView<T> sub = view.subview(slice);
    test::fuzz_check(sub.size() <= size, "a subview is not larger than its array");
    if (!sub.empty())
        test::fuzz_check(sub.data() >= elements.data() && sub.data() + sub.size() <= elements.data() + size,
                         "a subview stays inside its array");

    const size_t offset = provider.ConsumeIntegralInRange<size_t>(0, size + 8);
    const size_t count = provider.ConsumeIntegralInRange<size_t>(0, size + 8);
    const ArrayView<T> sized = view.subview(Slice::from_size(offset, count));
    if (count > 0 && offset + count <= size)
        test::fuzz_check(sized.size() == count && sized.data() == elements.data() + offset,
                         "from_size selects count elements at the offset");
    else if (count == 0 || offset >= size)
        test::fuzz_check(sized.empty(), "from_size outside the array selects nothing");
}

template <typename Element, typename Value>
void check_read_write(Array<Element> & array, std::vector<uint8_t> & shadow, size_t index, Value value)
{
    const size_t offset = index * sizeof(Element);
    const bool fits = offset + sizeof(Value) <= shadow.size();

    // a value overlapping the end of the array is not asserted: read_into accepts the part that exists
    Value loaded {};
    const bool readable = array_utils::read_into(array, index, loaded);
    test::fuzz_check(!fits || readable, "read_into reads a value that fits");
    test::fuzz_check(offset < shadow.size() || !readable, "read_into fails past the end");
    if (fits)
    {
        Value expected;
        std::memcpy(&expected, shadow.data() + offset, sizeof(Value));
        test::fuzz_check(loaded == expected, "read_into reads the bytes at the index");
    }
    bool threw = false;
    try
    {
        (void)array_utils::read<Value>(array, index);
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    test::fuzz_check(threw == !readable, "read throws exactly when read_into fails");

    test::fuzz_check(array_utils::write(array, index, value) == fits, "write succeeds exactly when it fits");
    if (fits)
        std::memcpy(shadow.data() + offset, &value, sizeof(Value));
}

template <typename Element>
void fuzz_read_write(FuzzedDataProvider & provider)
{
    const size_t size = provider.ConsumeIntegralInRange<size_t>(0, max_elements);
    std::vector<uint8_t> backing = test::consume_exact_bytes(provider, size * sizeof(Element));
    std::vector<uint8_t> shadow = backing;
    Array<Element> array;
    borrow(array, backing);

    for (size_t i = provider.ConsumeIntegralInRange<size_t>(1, max_operations); i > 0; --i)
    {
        const size_t index = provider.ConsumeIntegralInRange<size_t>(0, size + 4);
        switch (provider.ConsumeIntegralInRange<int>(0, 3))
        {
            case 0:
                check_read_write(array, shadow, index, provider.ConsumeIntegral<uint8_t>());
                break;
            case 1:
                check_read_write(array, shadow, index, provider.ConsumeIntegral<uint16_t>());
                break;
            case 2:
                check_read_write(array, shadow, index, provider.ConsumeIntegral<uint32_t>());
                break;
            default:
                check_read_write(array, shadow, index, provider.ConsumeIntegral<uint64_t>());
                break;
        }
        test::fuzz_check(backing == shadow, "write changes only the written bytes");
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 5))
    {
        case 0:
            fuzz_copy_bytes(provider);
            break;
        case 1:
            fuzz_slice_consistency(provider);
            break;
        case 2:
            fuzz_subview<uint8_t>(provider);
            break;
        case 3:
            fuzz_subview<uint32_t>(provider);
            break;
        case 4:
            fuzz_read_write<uint8_t>(provider);
            break;
        default:
            fuzz_read_write<uint32_t>(provider);
            break;
    }
    return 0;
}
