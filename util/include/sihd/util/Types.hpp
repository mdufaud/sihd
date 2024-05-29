#ifndef __SIHD_UTIL_DATATYPE_HPP__
#define __SIHD_UTIL_DATATYPE_HPP__

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

class Types
{
    public:
        Types() = delete;

        static size_t type_size(Type type);
        static const char *type_str(Type type);
        static Type from_str(std::string_view type);

        template <typename T>
        static constexpr bool is_same(Type type)
        {
            return Types::type<T>() == type;
        }

        template <typename T>
        static constexpr Type type()
        {
            return TYPE_OBJECT;
        }

        template <typename T>
        static constexpr const char *str()
        {
            return type_str(type<T>());
        }
};

template <>
Type Types::type<bool>();
template <>
Type Types::type<char>();
template <>
Type Types::type<int8_t>();
template <>
Type Types::type<uint8_t>();
template <>
Type Types::type<int16_t>();
template <>
Type Types::type<uint16_t>();
template <>
Type Types::type<int32_t>();
template <>
Type Types::type<uint32_t>();
template <>
Type Types::type<int64_t>();
template <>
Type Types::type<uint64_t>();
template <>
Type Types::type<float>();
template <>
Type Types::type<double>();

extern template Type Types::type<bool>();
extern template Type Types::type<char>();
extern template Type Types::type<int8_t>();
extern template Type Types::type<uint8_t>();
extern template Type Types::type<int16_t>();
extern template Type Types::type<uint16_t>();
extern template Type Types::type<int32_t>();
extern template Type Types::type<uint32_t>();
extern template Type Types::type<int64_t>();
extern template Type Types::type<uint64_t>();
extern template Type Types::type<float>();
extern template Type Types::type<double>();

} // namespace sihd::util

#endif