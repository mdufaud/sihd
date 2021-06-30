#ifndef __SIHD_UTIL_ENDIAN_HPP__
# define __SIHD_UTIL_ENDIAN_HPP__

# include <string>
extern "C" {
# include <stdint.h>
# include <byteswap.h>
};

namespace sihd::util
{

class Endian
{
    private:
        Endian() {};
        ~Endian() {};

        static std::string endian_type_to_str[];

    public:
        enum Endianness {
            unknown = 0,
            little = 1,
            big = 2,
        };

        static std::string type_to_string(Endianness type)
        {
            return Endian::endian_type_to_str[type];
        }

        static Endianness  get_endian();
        static bool    switch_buffer_endianness(void *buf, uint8_t size, size_t buf_size);

        template <typename T>
        static T   swap(T value)
        {
            return value;
        }

        /* Convert to native endianness */

        template <typename T, Endianness endian>
        static T   convert(T from)
        {
            return convert<T, endian>(from);
        }

        template <typename T>
        static T   convert(T value, Endianness endian)
        {
            if (endian == little)
                return convert<T, little>(value);
            else if (endian == big)
                return convert<T, big>(value);
            return value;
        }

        /* Convert from native to endianness */

        template <typename T, Endianness endian>
        static T   convert_from(T from)
        {
            return convert_from<T, endian>(from);
        }

        template <typename T>
        static T   convert_from(T value, Endianness endian)
        {
            if (endian == little)
                return convert_from<T, little>(value);
            else if (endian == big)
                return convert_from<T, big>(value);
            return value;
        } 
};

template <>
uint16_t    Endian::swap<uint16_t>(uint16_t ret);
template <>
int16_t     Endian::swap<int16_t>(int16_t ret);
template <>
uint32_t    Endian::swap<uint32_t>(uint32_t ret);
template <>
int32_t     Endian::swap<int32_t>(int32_t ret);
template <>
uint64_t    Endian::swap<uint64_t>(uint64_t ret);
template <>
int64_t     Endian::swap<int64_t>(int64_t ret);

template <>
float   Endian::swap<float>(float ret);

template <>
double   Endian::swap<double>(double ret);

template <>
uint16_t   Endian::convert<uint16_t, Endian::little>(uint16_t value);
template <>
uint16_t   Endian::convert<uint16_t, Endian::big>(uint16_t value);
template <>
int16_t    Endian::convert<int16_t, Endian::little>(int16_t value);
template <>
int16_t    Endian::convert<int16_t, Endian::big>(int16_t value);

template <>
uint32_t   Endian::convert<uint32_t, Endian::little>(uint32_t value);
template <>
uint32_t   Endian::convert<uint32_t, Endian::big>(uint32_t value);
template <>
int32_t    Endian::convert<int32_t, Endian::little>(int32_t value);
template <>
int32_t    Endian::convert<int32_t, Endian::big>(int32_t value);

template <>
uint64_t   Endian::convert<uint64_t, Endian::little>(uint64_t value);
template <>
uint64_t   Endian::convert<uint64_t, Endian::big>(uint64_t value);
template <>
int64_t    Endian::convert<int64_t, Endian::little>(int64_t value);
template <>
int64_t    Endian::convert<int64_t, Endian::big>(int64_t value);

template <>
uint16_t   Endian::convert_from<uint16_t, Endian::little>(uint16_t value);
template <>
uint16_t   Endian::convert_from<uint16_t, Endian::big>(uint16_t value);
template <>
int16_t    Endian::convert_from<int16_t, Endian::little>(int16_t value);
template <>
int16_t    Endian::convert_from<int16_t, Endian::big>(int16_t value);

template <>
uint32_t   Endian::convert_from<uint32_t, Endian::little>(uint32_t value);
template <>
uint32_t   Endian::convert_from<uint32_t, Endian::big>(uint32_t value);
template <>
int32_t    Endian::convert_from<int32_t, Endian::little>(int32_t value);
template <>
int32_t    Endian::convert_from<int32_t, Endian::big>(int32_t value);

template <>
uint64_t   Endian::convert_from<uint64_t, Endian::little>(uint64_t value);
template <>
uint64_t   Endian::convert_from<uint64_t, Endian::big>(uint64_t value);
template <>
int64_t    Endian::convert_from<int64_t, Endian::little>(int64_t value);
template <>
int64_t    Endian::convert_from<int64_t, Endian::big>(int64_t value);

}

#endif 