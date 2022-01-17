#include <sihd/util/Datatype.hpp>

namespace sihd::util
{

std::vector<std::string>     Datatype::dt_to_str = {
    "none", "bool", "char", "byte", "ubyte", "short", "ushort",
    "int", "uint", "long", "ulong", "float", "double",
    "string", "object"
};

std::map<std::string, Type>   Datatype::dt_from_str = {
    {"none", DNONE}, {"bool", DBOOL}, {"char", DCHAR}, {"byte", DBYTE}, {"ubyte", DUBYTE},
    {"short", DSHORT}, {"ushort", DUSHORT}, {"int", DINT}, {"uint", DUINT},
    {"long", DLONG}, {"ulong", DULONG}, {"float", DFLOAT}, {"double", DDOUBLE},
    {"string", DSTRING}, {"object", DOBJECT}
};

std::string  Datatype::datatype_to_string(Type type)
{
    return dt_to_str[type];
}

Type  Datatype::string_to_datatype(const std::string & type)
{
    return dt_from_str[type];
}

size_t  Datatype::datatype_size(Type type)
{
    switch (type)
    {
        case DBOOL:
            return sizeof(bool);
        case DCHAR:
            return sizeof(char);
        case DBYTE:
            return sizeof(int8_t);
        case DUBYTE:
            return sizeof(uint8_t);
        case DSHORT:
            return sizeof(int16_t);
        case DUSHORT:
            return sizeof(uint16_t);
        case DINT:
            return sizeof(int32_t);
        case DUINT:
            return sizeof(uint32_t);
        case DLONG:
            return sizeof(int64_t);
        case DULONG:
            return sizeof(uint64_t);
        case DFLOAT:
            return sizeof(float);
        case DDOUBLE:
            return sizeof(double);
        default:
            break ;
    }
    return 0;
}

template <>
Type    Datatype::type_to_datatype<bool>() { return DBOOL; };
template <>
Type    Datatype::type_to_datatype<char>() { return DCHAR; };
template <>
Type    Datatype::type_to_datatype<int8_t>() { return DBYTE; };
template <>
Type    Datatype::type_to_datatype<uint8_t>() { return DUBYTE; };
template <>
Type    Datatype::type_to_datatype<int16_t>() { return DSHORT; };
template <>
Type    Datatype::type_to_datatype<uint16_t>() { return DUSHORT; };
template <>
Type    Datatype::type_to_datatype<int32_t>() { return DINT; };
template <>
Type    Datatype::type_to_datatype<uint32_t>() { return DUINT; };
template <>
Type    Datatype::type_to_datatype<int64_t>() { return DLONG; };
template <>
Type    Datatype::type_to_datatype<uint64_t>() { return DULONG; };
template <>
Type    Datatype::type_to_datatype<float>() { return DFLOAT; };
template <>
Type    Datatype::type_to_datatype<double>() { return DDOUBLE; };
template <>
Type    Datatype::type_to_datatype<std::string>() { return DSTRING; };

}