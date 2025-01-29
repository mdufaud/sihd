#ifndef __SIHD_UTIL_ENDIAN_HPP__
#define __SIHD_UTIL_ENDIAN_HPP__

#include <bit>
#include <cstdint>
#include <string>

#include <sihd/util/portable_endian.h>

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

// byte swap
template <typename T>
T swap(T value);

/* Convert from little or big-endian order to host byte order */
template <typename T, std::endian endian>
T convert(T from);

/* Convert from host byte order to little or big-endian order */
template <typename T, std::endian endian>
T convert_from(T value);

/* Convert from little or big-endian order to host byte order */
template <typename T>
T convert(T value, std::endian endian)
{
    if (endian == std::endian::little)
        return convert<T, std::endian::little>(value);
    else if (endian == std::endian::big)
        return convert<T, std::endian::big>(value);
    return value;
}

/* Convert from host byte order to little or big-endian order */
template <typename T>
T convert_from(T value, std::endian endian)
{
    if (endian == std::endian::little)
        return convert_from<T, std::endian::little>(value);
    else if (endian == std::endian::big)
        return convert_from<T, std::endian::big>(value);
    return value;
}

} // namespace endian

} // namespace sihd::util

template <>
uint16_t sihd::util::endian::swap<uint16_t>(uint16_t ret);
template <>
int16_t sihd::util::endian::swap<int16_t>(int16_t ret);
template <>
uint32_t sihd::util::endian::swap<uint32_t>(uint32_t ret);
template <>
int32_t sihd::util::endian::swap<int32_t>(int32_t ret);
template <>
uint64_t sihd::util::endian::swap<uint64_t>(uint64_t ret);
template <>
int64_t sihd::util::endian::swap<int64_t>(int64_t ret);

template <>
float sihd::util::endian::swap<float>(float ret);

template <>
double sihd::util::endian::swap<double>(double ret);

// convert from little or big-endian order to host byte order.

template <>
uint16_t sihd::util::endian::convert<uint16_t, std::endian::little>(uint16_t value);
template <>
uint16_t sihd::util::endian::convert<uint16_t, std::endian::big>(uint16_t value);
template <>
int16_t sihd::util::endian::convert<int16_t, std::endian::little>(int16_t value);
template <>
int16_t sihd::util::endian::convert<int16_t, std::endian::big>(int16_t value);

template <>
uint32_t sihd::util::endian::convert<uint32_t, std::endian::little>(uint32_t value);
template <>
uint32_t sihd::util::endian::convert<uint32_t, std::endian::big>(uint32_t value);
template <>
int32_t sihd::util::endian::convert<int32_t, std::endian::little>(int32_t value);
template <>
int32_t sihd::util::endian::convert<int32_t, std::endian::big>(int32_t value);

template <>
uint64_t sihd::util::endian::convert<uint64_t, std::endian::little>(uint64_t value);
template <>
uint64_t sihd::util::endian::convert<uint64_t, std::endian::big>(uint64_t value);
template <>
int64_t sihd::util::endian::convert<int64_t, std::endian::little>(int64_t value);
template <>
int64_t sihd::util::endian::convert<int64_t, std::endian::big>(int64_t value);

// convert from host byte order to little or big-endian order

template <>
uint16_t sihd::util::endian::convert_from<uint16_t, std::endian::little>(uint16_t value);
template <>
uint16_t sihd::util::endian::convert_from<uint16_t, std::endian::big>(uint16_t value);
template <>
int16_t sihd::util::endian::convert_from<int16_t, std::endian::little>(int16_t value);
template <>
int16_t sihd::util::endian::convert_from<int16_t, std::endian::big>(int16_t value);

template <>
uint32_t sihd::util::endian::convert_from<uint32_t, std::endian::little>(uint32_t value);
template <>
uint32_t sihd::util::endian::convert_from<uint32_t, std::endian::big>(uint32_t value);
template <>
int32_t sihd::util::endian::convert_from<int32_t, std::endian::little>(int32_t value);
template <>
int32_t sihd::util::endian::convert_from<int32_t, std::endian::big>(int32_t value);

template <>
uint64_t sihd::util::endian::convert_from<uint64_t, std::endian::little>(uint64_t value);
template <>
uint64_t sihd::util::endian::convert_from<uint64_t, std::endian::big>(uint64_t value);
template <>
int64_t sihd::util::endian::convert_from<int64_t, std::endian::little>(int64_t value);
template <>
int64_t sihd::util::endian::convert_from<int64_t, std::endian::big>(int64_t value);

#endif