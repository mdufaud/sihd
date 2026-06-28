#include <cstring>
#include <map>

#include <sihd/util/container.hpp>
#include <sihd/util/type.hpp>

#define STR_TYPE_NONE "none"
#define STR_TYPE_BOOL "bool"
#define STR_TYPE_CHAR "char"
#define STR_TYPE_BYTE "byte"
#define STR_TYPE_UBYTE "ubyte"
#define STR_TYPE_SHORT "short"
#define STR_TYPE_USHORT "ushort"
#define STR_TYPE_INT "int"
#define STR_TYPE_UINT "uint"
#define STR_TYPE_LONG "long"
#define STR_TYPE_ULONG "ulong"
#define STR_TYPE_FLOAT "float"
#define STR_TYPE_DOUBLE "double"
#define STR_TYPE_OBJECT "object"

namespace sihd::util::type
{

sihd::util::Type from_str(std::string_view type)
{
    static std::map<std::string_view, sihd::util::Type> str_to_type = {{STR_TYPE_NONE, TYPE_NONE},
                                                                       {STR_TYPE_BOOL, TYPE_BOOL},
                                                                       {STR_TYPE_CHAR, TYPE_CHAR},
                                                                       {STR_TYPE_BYTE, TYPE_BYTE},
                                                                       {STR_TYPE_UBYTE, TYPE_UBYTE},
                                                                       {STR_TYPE_SHORT, TYPE_SHORT},
                                                                       {STR_TYPE_USHORT, TYPE_USHORT},
                                                                       {STR_TYPE_INT, TYPE_INT},
                                                                       {STR_TYPE_UINT, TYPE_UINT},
                                                                       {STR_TYPE_LONG, TYPE_LONG},
                                                                       {STR_TYPE_ULONG, TYPE_ULONG},
                                                                       {STR_TYPE_FLOAT, TYPE_FLOAT},
                                                                       {STR_TYPE_DOUBLE, TYPE_DOUBLE},
                                                                       {STR_TYPE_OBJECT, TYPE_OBJECT}};
    return container::get_or(str_to_type, type, TYPE_NONE);
}

const char *str(sihd::util::Type type)
{
    switch (type)
    {
        case TYPE_NONE:
            return STR_TYPE_NONE;
        case TYPE_BOOL:
            return STR_TYPE_BOOL;
        case TYPE_CHAR:
            return STR_TYPE_CHAR;
        case TYPE_BYTE:
            return STR_TYPE_BYTE;
        case TYPE_UBYTE:
            return STR_TYPE_UBYTE;
        case TYPE_SHORT:
            return STR_TYPE_SHORT;
        case TYPE_USHORT:
            return STR_TYPE_USHORT;
        case TYPE_INT:
            return STR_TYPE_INT;
        case TYPE_UINT:
            return STR_TYPE_UINT;
        case TYPE_LONG:
            return STR_TYPE_LONG;
        case TYPE_ULONG:
            return STR_TYPE_ULONG;
        case TYPE_FLOAT:
            return STR_TYPE_FLOAT;
        case TYPE_DOUBLE:
            return STR_TYPE_DOUBLE;
        case TYPE_OBJECT:
            return STR_TYPE_OBJECT;
        default:
            break;
    }
    return "unknown";
}

size_t size(sihd::util::Type type)
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
            break;
    }
    return 0;
}

template <>
sihd::util::Type from<bool>()
{
    return TYPE_BOOL;
};

template <>
sihd::util::Type from<char>()
{
    return TYPE_CHAR;
};

template <>
sihd::util::Type from<int8_t>()
{
    return TYPE_BYTE;
};

template <>
sihd::util::Type from<uint8_t>()
{
    return TYPE_UBYTE;
};

template <>
sihd::util::Type from<int16_t>()
{
    return TYPE_SHORT;
};

template <>
sihd::util::Type from<uint16_t>()
{
    return TYPE_USHORT;
};

template <>
sihd::util::Type from<int32_t>()
{
    return TYPE_INT;
};

template <>
sihd::util::Type from<uint32_t>()
{
    return TYPE_UINT;
};

template <>
sihd::util::Type from<int64_t>()
{
    return TYPE_LONG;
};

template <>
sihd::util::Type from<uint64_t>()
{
    return TYPE_ULONG;
};

template <>
sihd::util::Type from<float>()
{
    return TYPE_FLOAT;
};

template <>
sihd::util::Type from<double>()
{
    return TYPE_DOUBLE;
};

} // namespace sihd::util::type