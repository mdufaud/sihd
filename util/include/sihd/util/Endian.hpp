#ifndef __SIHD_UTIL_ENDIAN_HPP__
#define __SIHD_UTIL_ENDIAN_HPP__

#include <cstdint>
#include <string>

#include <sihd/util/portable_endian.h>

namespace sihd::util
{

// These functions convert the byte encoding of integer values from the byte order that the current CPU (the "host")
// uses, to and from little-endian and big-endian byte order.

class Endian
{
    private:
        Endian() = default;
        ~Endian() = default;

    public:
        enum Endianness
        {
            UNKNOWN,
            LITTLE,
            BIG,
        };

        static constexpr Endianness endian()
        {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            return LITTLE;
#elif __BYTE_ORDER == __BIG_ENDIAN
            return BIG;
#else
            return UNKNOWN;
#endif
        }
        static std::string type_str(Endianness type);
        static bool switch_buffer_endianness(void *buf, uint8_t size, size_t buf_size);

        // error if trying to swap non defined templates
        template <typename T>
        static T swap(T value);

        /* Convert from little or big-endian order to host byte order */

        template <typename T, Endianness Endian>
        static T convert(T from)
        {
            return convert<T, Endian>(from);
        }

        template <typename T>
        static T convert(T value, Endianness endian)
        {
            if (endian == LITTLE)
                return convert<T, LITTLE>(value);
            else if (endian == BIG)
                return convert<T, BIG>(value);
            return value;
        }

        /* Convert from host byte order to little or big-endian order */

        template <typename T>
        static T convert_from(T value, Endianness endian)
        {
            if (endian == LITTLE)
                return convert_from<T, LITTLE>(value);
            else if (endian == BIG)
                return convert_from<T, BIG>(value);
            return value;
        }

        // error if trying to convert from non defined templates
        template <typename T, Endianness Endian>
        static T convert_from(T value);
};

template <>
uint16_t Endian::swap<uint16_t>(uint16_t ret);
template <>
int16_t Endian::swap<int16_t>(int16_t ret);
template <>
uint32_t Endian::swap<uint32_t>(uint32_t ret);
template <>
int32_t Endian::swap<int32_t>(int32_t ret);
template <>
uint64_t Endian::swap<uint64_t>(uint64_t ret);
template <>
int64_t Endian::swap<int64_t>(int64_t ret);

template <>
float Endian::swap<float>(float ret);

template <>
double Endian::swap<double>(double ret);

// convert from little or big-endian order to host byte order.

template <>
uint16_t Endian::convert<uint16_t, Endian::LITTLE>(uint16_t value);
template <>
uint16_t Endian::convert<uint16_t, Endian::BIG>(uint16_t value);
template <>
int16_t Endian::convert<int16_t, Endian::LITTLE>(int16_t value);
template <>
int16_t Endian::convert<int16_t, Endian::BIG>(int16_t value);

template <>
uint32_t Endian::convert<uint32_t, Endian::LITTLE>(uint32_t value);
template <>
uint32_t Endian::convert<uint32_t, Endian::BIG>(uint32_t value);
template <>
int32_t Endian::convert<int32_t, Endian::LITTLE>(int32_t value);
template <>
int32_t Endian::convert<int32_t, Endian::BIG>(int32_t value);

template <>
uint64_t Endian::convert<uint64_t, Endian::LITTLE>(uint64_t value);
template <>
uint64_t Endian::convert<uint64_t, Endian::BIG>(uint64_t value);
template <>
int64_t Endian::convert<int64_t, Endian::LITTLE>(int64_t value);
template <>
int64_t Endian::convert<int64_t, Endian::BIG>(int64_t value);

// convert from host byte order to little or big-endian order

template <>
uint16_t Endian::convert_from<uint16_t, Endian::LITTLE>(uint16_t value);
template <>
uint16_t Endian::convert_from<uint16_t, Endian::BIG>(uint16_t value);
template <>
int16_t Endian::convert_from<int16_t, Endian::LITTLE>(int16_t value);
template <>
int16_t Endian::convert_from<int16_t, Endian::BIG>(int16_t value);

template <>
uint32_t Endian::convert_from<uint32_t, Endian::LITTLE>(uint32_t value);
template <>
uint32_t Endian::convert_from<uint32_t, Endian::BIG>(uint32_t value);
template <>
int32_t Endian::convert_from<int32_t, Endian::LITTLE>(int32_t value);
template <>
int32_t Endian::convert_from<int32_t, Endian::BIG>(int32_t value);

template <>
uint64_t Endian::convert_from<uint64_t, Endian::LITTLE>(uint64_t value);
template <>
uint64_t Endian::convert_from<uint64_t, Endian::BIG>(uint64_t value);
template <>
int64_t Endian::convert_from<int64_t, Endian::LITTLE>(int64_t value);
template <>
int64_t Endian::convert_from<int64_t, Endian::BIG>(int64_t value);

} // namespace sihd::util

#endif