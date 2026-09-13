#ifndef __SIHD_UTIL_TYPE_HPP__
#define __SIHD_UTIL_TYPE_HPP__

#include <cstdint>
#include <string_view>

namespace sihd::util
{

enum Type
{
    TYPE_NONE,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_BYTE,
    TYPE_UBYTE,
    TYPE_SHORT,
    TYPE_USHORT,
    TYPE_INT,
    TYPE_UINT,
    TYPE_LONG,
    TYPE_ULONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_OBJECT,
};

namespace type
{

size_t size(Type type);
const char *str(Type type);
bool is_unsigned(Type type);
Type from_str(std::string_view type);

template <typename T>
constexpr Type from()
{
    return TYPE_OBJECT;
}

template <typename T>
constexpr bool is_same(Type type)
{
    return type::from<T>() == type;
}

template <typename T>
constexpr const char *str()
{
    return type::str(type::from<T>());
}

template <>
constexpr Type from<bool>()
{
    return TYPE_BOOL;
}

template <>
constexpr Type from<char>()
{
    return TYPE_CHAR;
}

template <>
constexpr Type from<int8_t>()
{
    return TYPE_BYTE;
}

template <>
constexpr Type from<uint8_t>()
{
    return TYPE_UBYTE;
}

template <>
constexpr Type from<int16_t>()
{
    return TYPE_SHORT;
}

template <>
constexpr Type from<uint16_t>()
{
    return TYPE_USHORT;
}

template <>
constexpr Type from<int32_t>()
{
    return TYPE_INT;
}

template <>
constexpr Type from<uint32_t>()
{
    return TYPE_UINT;
}

template <>
constexpr Type from<int64_t>()
{
    return TYPE_LONG;
}

template <>
constexpr Type from<uint64_t>()
{
    return TYPE_ULONG;
}

template <>
constexpr Type from<float>()
{
    return TYPE_FLOAT;
}

template <>
constexpr Type from<double>()
{
    return TYPE_DOUBLE;
}

// int64_t aliases either long or long long depending on the ABI
#if __SIZEOF_LONG__ == 4
template <>
constexpr Type from<long>()
{
    return TYPE_INT;
}

template <>
constexpr Type from<unsigned long>()
{
    return TYPE_UINT;
}
#else
template <>
constexpr Type from<long long>()
{
    return TYPE_LONG;
}

template <>
constexpr Type from<unsigned long long>()
{
    return TYPE_ULONG;
}
#endif

} // namespace type

} // namespace sihd::util

#endif