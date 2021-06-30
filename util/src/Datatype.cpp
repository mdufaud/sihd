#include <sihd/util/Datatype.hpp>

namespace sihd::util
{

std::vector<std::string>     Datatype::v_datatype_to_string = {
    "NONE", "BYTE", "UBYTE", "SHORT", "USHORT",
    "INT", "UINT", "LONG", "ULONG", "FLOAT", "DOUBLE",
    "BOOL", "STRING", "OBJECT"
};

std::map<std::string, e_datatype>   Datatype::m_datatype_from_string = {
    {"BYTE", BYTE}, {"UBYTE", UBYTE},
    {"SHORT", SHORT}, {"USHORT", USHORT}, {"INT", INT}, {"UINT", UINT}, 
    {"LONG", LONG}, {"ULONG", ULONG}, {"FLOAT", FLOAT}, {"DOUBLE", DOUBLE},
    {"BOOL", BOOL}, {"STRING", STRING}, {"OBJECT", OBJECT}
};

std::string  Datatype::datatype_to_string(e_datatype type)
{
    return v_datatype_to_string[type];
}

e_datatype  Datatype::string_to_datatype(const std::string & type)
{
    return m_datatype_from_string[type];
}

template <>
e_datatype    Datatype::type_to_datatype<int8_t>() { return BYTE; };
template <>
e_datatype    Datatype::type_to_datatype<uint8_t>() { return UBYTE; };
template <>
e_datatype    Datatype::type_to_datatype<int16_t>() { return SHORT; };
template <>
e_datatype    Datatype::type_to_datatype<uint16_t>() { return USHORT; };
template <>
e_datatype    Datatype::type_to_datatype<int32_t>() { return INT; };
template <>
e_datatype    Datatype::type_to_datatype<uint32_t>() { return UINT; };
template <>
e_datatype    Datatype::type_to_datatype<int64_t>() { return LONG; };
template <>
e_datatype    Datatype::type_to_datatype<uint64_t>() { return ULONG; };
template <>
e_datatype    Datatype::type_to_datatype<float>() { return FLOAT; };
template <>
e_datatype    Datatype::type_to_datatype<double>() { return DOUBLE; };
template <>
e_datatype    Datatype::type_to_datatype<bool>() { return BOOL; };
template <>
e_datatype    Datatype::type_to_datatype<std::string>() { return STRING; };

}