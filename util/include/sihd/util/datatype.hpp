#ifndef __SIHD_UTIL_DATATYPE_HPP__
# define __SIHD_UTIL_DATATYPE_HPP__

# include <string>
# include <variant>

namespace sihd::util::datatype
{


typedef std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
int64_t, uint64_t, float, double, bool, std::string>    DataTypeVariant;

enum DataType {
    NONE,
    BYTE,
    UBYTE,
    SHORT,
    USHORT,
    INT,
    UINT,
    LONG,
    ULONG,
    FLOAT,
    DOUBLE,
    BOOL,
    STRING,
    OBJECT,
};

std::string  datatype_to_string(DataType type);
DataType     string_to_datatype(const std::string & type);

template <typename T>
DataType     type_to_datatype()
{
    return OBJECT;
}

}

#endif 