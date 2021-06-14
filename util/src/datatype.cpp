#include <sihd/util/datatype.hpp>

#include <vector>
#include <map>

namespace sihd::util::datatype
{

static std::vector<std::string>     _datatype_to_string = {
    "NONE", "BYTE", "UBYTE", "SHORT", "USHORT",
    "INT", "UINT", "LONG", "ULONG", "FLOAT", "DOUBLE",
    "BOOL", "STRING", "OBJECT"
};

static std::map<std::string, DataType>   _datatype_from_string = {
    {"BYTE", BYTE}, {"UBYTE", UBYTE},
    {"SHORT", SHORT}, {"USHORT", USHORT}, {"INT", INT}, {"UINT", UINT}, 
    {"LONG", LONG}, {"ULONG", ULONG}, {"FLOAT", FLOAT}, {"DOUBLE", DOUBLE},
    {"BOOL", BOOL}, {"STRING", STRING}, {"OBJECT", OBJECT}
};

std::string  datatype_to_string(DataType type)
{
    return _datatype_to_string[type];
}

DataType     string_to_datatype(const std::string & type)
{
    return _datatype_from_string[type];
}

template <>
DataType    type_to_datatype<int8_t>() { return BYTE; };
template <>
DataType    type_to_datatype<uint8_t>() { return UBYTE; };
template <>
DataType    type_to_datatype<int16_t>() { return SHORT; };
template <>
DataType    type_to_datatype<uint16_t>() { return USHORT; };
template <>
DataType    type_to_datatype<int32_t>() { return INT; };
template <>
DataType    type_to_datatype<uint32_t>() { return UINT; };
template <>
DataType    type_to_datatype<int64_t>() { return LONG; };
template <>
DataType    type_to_datatype<uint64_t>() { return ULONG; };
template <>
DataType    type_to_datatype<float>() { return FLOAT; };
template <>
DataType    type_to_datatype<double>() { return DOUBLE; };
template <>
DataType    type_to_datatype<bool>() { return BOOL; };
template <>
DataType    type_to_datatype<std::string>() { return STRING; };

}