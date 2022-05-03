#ifndef __SIHD_UTIL_DATATYPE_HPP__
# define __SIHD_UTIL_DATATYPE_HPP__

# include <string>
# include <vector>
# include <map>

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
    private:
        ~Types() {};

    public:
        static size_t type_size(Type type);
        static const char *type_to_string(Type type);
        static Type string_to_type(std::string_view type);

        template <typename T>
        static Type to_type()
        {
            return TYPE_OBJECT;
        }

        template <typename T>
        static const char *to_string()
        {
            return type_to_string(to_type<T>());
        }
};

template <>
Type    Types::to_type<bool>();
template <>
Type    Types::to_type<char>();
template <>
Type    Types::to_type<int8_t>();
template <>
Type    Types::to_type<uint8_t>();
template <>
Type    Types::to_type<int16_t>();
template <>
Type    Types::to_type<uint16_t>();
template <>
Type    Types::to_type<int32_t>();
template <>
Type    Types::to_type<uint32_t>();
template <>
Type    Types::to_type<int64_t>();
template <>
Type    Types::to_type<uint64_t>();
template <>
Type    Types::to_type<float>();
template <>
Type    Types::to_type<double>();
template <>
Type    Types::to_type<std::string>();

}

#endif