#ifndef __SIHD_UTIL_ENDIAN_HPP__
#define __SIHD_UTIL_ENDIAN_HPP__

#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <string>

namespace sihd::util
{

enum class ByteSize
{
    b2 = 2,
    b4 = 4,
    b8 = 8
};

namespace endian
{

// These functions convert the byte encoding of integer values from the byte order that the current CPU (the "host")
// uses, to and from little-endian and big-endian byte order.

std::string type_str(std::endian type);

bool switch_buffer_endianness(void *buf, ByteSize size, size_t buf_size);

template <typename T>
concept SwappableEndian = std::integral<T> || std::same_as<T, float> || std::same_as<T, double>;

// byte swap
template <typename T>
    requires SwappableEndian<T>
constexpr T swap(T value)
{
    if constexpr (std::integral<T>)
        return std::byteswap(value);
    else
    {
        using Uint = std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t, uint64_t>;
        return std::bit_cast<T>(std::byteswap(std::bit_cast<Uint>(value)));
    }
}

/* Convert from little or big-endian order to host byte order */
template <typename T, std::endian endian>
    requires SwappableEndian<T>
constexpr T convert(T from)
{
    if constexpr (endian == std::endian::native)
        return from;
    else
        return swap(from);
}

/* Convert from host byte order to little or big-endian order */
template <typename T, std::endian endian>
    requires SwappableEndian<T>
constexpr T convert_from(T value)
{
    if constexpr (endian == std::endian::native)
        return value;
    else
        return swap(value);
}

/* Convert from little or big-endian order to host byte order */
template <typename T>
constexpr T convert(T value, std::endian endian)
{
    if (endian == std::endian::little)
        return convert<T, std::endian::little>(value);
    else if (endian == std::endian::big)
        return convert<T, std::endian::big>(value);
    return value;
}

/* Convert from host byte order to little or big-endian order */
template <typename T>
constexpr T convert_from(T value, std::endian endian)
{
    if (endian == std::endian::little)
        return convert_from<T, std::endian::little>(value);
    else if (endian == std::endian::big)
        return convert_from<T, std::endian::big>(value);
    return value;
}

/* Read 'size' little-endian bytes into T, zero-padded above */
template <typename T>
    requires SwappableEndian<T>
T read_le(const uint8_t *data, size_t size)
{
    T value = 0;
    std::memcpy(&value, data, size);
    return convert<T, std::endian::little>(value);
}

/* Read 'size' big-endian bytes into T, zero-padded above */
template <typename T>
    requires SwappableEndian<T>
T read_be(const uint8_t *data, size_t size)
{
    T value = 0;
    std::memcpy(reinterpret_cast<uint8_t *>(&value) + (sizeof(T) - size), data, size);
    return convert<T, std::endian::big>(value);
}

} // namespace endian

} // namespace sihd::util

#endif
