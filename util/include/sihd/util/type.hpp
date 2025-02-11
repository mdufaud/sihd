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

} // namespace type

} // namespace sihd::util

template <>
sihd::util::Type sihd::util::type::from<bool>();
template <>
sihd::util::Type sihd::util::type::from<char>();
template <>
sihd::util::Type sihd::util::type::from<int8_t>();
template <>
sihd::util::Type sihd::util::type::from<uint8_t>();
template <>
sihd::util::Type sihd::util::type::from<int16_t>();
template <>
sihd::util::Type sihd::util::type::from<uint16_t>();
template <>
sihd::util::Type sihd::util::type::from<int32_t>();
template <>
sihd::util::Type sihd::util::type::from<uint32_t>();
template <>
sihd::util::Type sihd::util::type::from<int64_t>();
template <>
sihd::util::Type sihd::util::type::from<uint64_t>();
template <>
sihd::util::Type sihd::util::type::from<float>();
template <>
sihd::util::Type sihd::util::type::from<double>();

extern template sihd::util::Type sihd::util::type::from<bool>();
extern template sihd::util::Type sihd::util::type::from<char>();
extern template sihd::util::Type sihd::util::type::from<int8_t>();
extern template sihd::util::Type sihd::util::type::from<uint8_t>();
extern template sihd::util::Type sihd::util::type::from<int16_t>();
extern template sihd::util::Type sihd::util::type::from<uint16_t>();
extern template sihd::util::Type sihd::util::type::from<int32_t>();
extern template sihd::util::Type sihd::util::type::from<uint32_t>();
extern template sihd::util::Type sihd::util::type::from<int64_t>();
extern template sihd::util::Type sihd::util::type::from<uint64_t>();
extern template sihd::util::Type sihd::util::type::from<float>();
extern template sihd::util::Type sihd::util::type::from<double>();

#endif