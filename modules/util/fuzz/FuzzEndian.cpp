#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <sihd/util/endian.hpp>

#include "fuzz_helper.hpp"
#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::util;

namespace
{
constexpr size_t max_elements = 32;

std::vector<uint8_t> reverse_each(const std::vector<uint8_t> & bytes, size_t width)
{
    std::vector<uint8_t> reversed = bytes;
    for (size_t i = 0; i + width <= reversed.size(); i += width)
        std::reverse(reversed.begin() + i, reversed.begin() + i + width);
    return reversed;
}

void fuzz_switch_buffer(FuzzedDataProvider & provider)
{
    const int width = provider.ConsumeIntegralInRange<int>(0, 16);
    const bool valid_width = width == 2 || width == 4 || width == 8;
    const size_t elements = provider.ConsumeIntegralInRange<size_t>(1, max_elements);
    const size_t element_size = valid_width ? static_cast<size_t>(width) : 1;
    std::vector<uint8_t> buffer = test::consume_exact_bytes(provider, elements * element_size);
    const std::vector<uint8_t> original = buffer;

    const bool switched = endian::switch_buffer_endianness(buffer.data(), static_cast<ByteSize>(width), elements);
    test::fuzz_check(switched == valid_width, "only 2, 4 and 8 byte elements are accepted");
    if (!valid_width)
    {
        test::fuzz_check(buffer == original, "a rejected width leaves the buffer untouched");
        return;
    }
    test::fuzz_check(buffer == reverse_each(original, element_size), "each element has its bytes reversed");
    test::fuzz_check(endian::switch_buffer_endianness(buffer.data(), static_cast<ByteSize>(width), elements),
                     "the second switch is accepted too");
    test::fuzz_check(buffer == original, "switching twice restores the buffer");
}

template <typename T>
void check_swap(T value)
{
    std::vector<uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
    std::reverse(bytes.begin(), bytes.end());
    const T swapped = endian::swap(value);
    test::fuzz_check(std::memcmp(&swapped, bytes.data(), sizeof(T)) == 0, "swap reverses the bytes");
    const T restored = endian::swap(swapped);
    test::fuzz_check(std::memcmp(&restored, &value, sizeof(T)) == 0, "swap twice restores the value");
}

void fuzz_swap(FuzzedDataProvider & provider)
{
    check_swap(provider.ConsumeIntegral<uint16_t>());
    check_swap(provider.ConsumeIntegral<uint32_t>());
    check_swap(provider.ConsumeIntegral<uint64_t>());
    check_swap(provider.ConsumeFloatingPoint<float>());
    check_swap(provider.ConsumeFloatingPoint<double>());
}

template <typename T>
void check_read(FuzzedDataProvider & provider)
{
    const size_t size = provider.ConsumeIntegralInRange<size_t>(1, sizeof(T));
    const std::vector<uint8_t> bytes = test::consume_exact_bytes(provider, size);

    T little = 0;
    T big = 0;
    for (size_t i = 0; i < size; ++i)
    {
        little |= static_cast<T>(static_cast<T>(bytes[i]) << (8 * i));
        big = static_cast<T>((big << 8) | bytes[i]);
    }
    test::fuzz_check(endian::read_le<T>(bytes.data(), size) == little, "read_le reads the low bytes first");
    test::fuzz_check(endian::read_be<T>(bytes.data(), size) == big, "read_be reads the high bytes first");
}

void fuzz_read(FuzzedDataProvider & provider)
{
    check_read<uint16_t>(provider);
    check_read<uint32_t>(provider);
    check_read<uint64_t>(provider);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 2))
    {
        case 0:
            fuzz_switch_buffer(provider);
            break;
        case 1:
            fuzz_swap(provider);
            break;
        default:
            fuzz_read(provider);
            break;
    }
    return 0;
}
