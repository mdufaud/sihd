#include <sihd/util/Datatype.hpp>

namespace sihd::util
{

std::vector<std::string>     Datatype::v_datatype_to_string = {
    "NONE", "BOOL", "CHAR", "BYTE", "UBYTE", "SHORT", "USHORT",
    "INT", "UINT", "LONG", "ULONG", "FLOAT", "DOUBLE",
    "STRING", "OBJECT"
};

std::map<std::string, Datatypes>   Datatype::m_datatype_from_string = {
    {"NONE", NONE}, {"BOOL", BOOL}, {"CHAR", CHAR}, {"BYTE", BYTE}, {"UBYTE", UBYTE},
    {"SHORT", SHORT}, {"USHORT", USHORT}, {"INT", INT}, {"UINT", UINT}, 
    {"LONG", LONG}, {"ULONG", ULONG}, {"FLOAT", FLOAT}, {"DOUBLE", DOUBLE},
    {"STRING", STRING}, {"OBJECT", OBJECT}
};

std::string  Datatype::datatype_to_string(Datatypes type)
{
    return v_datatype_to_string[type];
}

Datatypes  Datatype::string_to_datatype(const std::string & type)
{
    return m_datatype_from_string[type];
}

size_t  Datatype::datatype_size(Datatypes type)
{
    switch (type)
    {
        case BOOL:
            return sizeof(bool);
        case CHAR:
            return sizeof(char);
        case BYTE:
            return sizeof(int8_t);
        case UBYTE:
            return sizeof(uint8_t);
        case SHORT:
            return sizeof(int16_t);
        case USHORT:
            return sizeof(uint16_t);
        case INT:
            return sizeof(int32_t);
        case UINT:
            return sizeof(uint32_t);
        case LONG:
            return sizeof(int64_t);
        case ULONG:
            return sizeof(uint64_t);
        case FLOAT:
            return sizeof(float);
        case DOUBLE:
            return sizeof(double);
        default:
            break ;
    }
    return 0;
}

template <>
Datatypes    Datatype::type_to_datatype<bool>() { return BOOL; };
template <>
Datatypes    Datatype::type_to_datatype<char>() { return CHAR; };
template <>
Datatypes    Datatype::type_to_datatype<int8_t>() { return BYTE; };
template <>
Datatypes    Datatype::type_to_datatype<uint8_t>() { return UBYTE; };
template <>
Datatypes    Datatype::type_to_datatype<int16_t>() { return SHORT; };
template <>
Datatypes    Datatype::type_to_datatype<uint16_t>() { return USHORT; };
template <>
Datatypes    Datatype::type_to_datatype<int32_t>() { return INT; };
template <>
Datatypes    Datatype::type_to_datatype<uint32_t>() { return UINT; };
template <>
Datatypes    Datatype::type_to_datatype<int64_t>() { return LONG; };
template <>
Datatypes    Datatype::type_to_datatype<uint64_t>() { return ULONG; };
template <>
Datatypes    Datatype::type_to_datatype<float>() { return FLOAT; };
template <>
Datatypes    Datatype::type_to_datatype<double>() { return DOUBLE; };
template <>
Datatypes    Datatype::type_to_datatype<std::string>() { return STRING; };

}