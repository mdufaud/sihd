#ifndef __SIHD_UTIL_TYPE_HPP__
#define __SIHD_UTIL_TYPE_HPP__

#include <cstddef>
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

constexpr Type from_str(std::string_view str)
{
    if (str == "none")
        return TYPE_NONE;
    if (str == "bool")
        return TYPE_BOOL;
    if (str == "char")
        return TYPE_CHAR;
    if (str == "byte")
        return TYPE_BYTE;
    if (str == "ubyte")
        return TYPE_UBYTE;
    if (str == "short")
        return TYPE_SHORT;
    if (str == "ushort")
        return TYPE_USHORT;
    if (str == "int")
        return TYPE_INT;
    if (str == "uint")
        return TYPE_UINT;
    if (str == "long")
        return TYPE_LONG;
    if (str == "ulong")
        return TYPE_ULONG;
    if (str == "float")
        return TYPE_FLOAT;
    if (str == "double")
        return TYPE_DOUBLE;
    if (str == "object")
        return TYPE_OBJECT;
    return TYPE_NONE;
}

constexpr const char *str(Type type)
{
    switch (type)
    {
        case TYPE_NONE:
            return "none";
        case TYPE_BOOL:
            return "bool";
        case TYPE_CHAR:
            return "char";
        case TYPE_BYTE:
            return "byte";
        case TYPE_UBYTE:
            return "ubyte";
        case TYPE_SHORT:
            return "short";
        case TYPE_USHORT:
            return "ushort";
        case TYPE_INT:
            return "int";
        case TYPE_UINT:
            return "uint";
        case TYPE_LONG:
            return "long";
        case TYPE_ULONG:
            return "ulong";
        case TYPE_FLOAT:
            return "float";
        case TYPE_DOUBLE:
            return "double";
        case TYPE_OBJECT:
            return "object";
        default:
            return "unknown";
    }
}

constexpr size_t size(Type type)
{
    switch (type)
    {
        case TYPE_BOOL:
            return sizeof(bool);
        case TYPE_CHAR:
            return sizeof(char);
        case TYPE_BYTE:
            return sizeof(int8_t);
        case TYPE_UBYTE:
            return sizeof(uint8_t);
        case TYPE_SHORT:
            return sizeof(int16_t);
        case TYPE_USHORT:
            return sizeof(uint16_t);
        case TYPE_INT:
            return sizeof(int32_t);
        case TYPE_UINT:
            return sizeof(uint32_t);
        case TYPE_LONG:
            return sizeof(int64_t);
        case TYPE_ULONG:
            return sizeof(uint64_t);
        case TYPE_FLOAT:
            return sizeof(float);
        case TYPE_DOUBLE:
            return sizeof(double);
        default:
            return 0;
    }
}

constexpr bool is_unsigned(Type type)
{
    switch (type)
    {
        case TYPE_UBYTE:
        case TYPE_USHORT:
        case TYPE_UINT:
        case TYPE_ULONG:
            return true;
        default:
            return false;
    }
}

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