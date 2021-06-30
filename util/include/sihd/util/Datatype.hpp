#ifndef __SIHD_UTIL_DATATYPE_HPP__
# define __SIHD_UTIL_DATATYPE_HPP__

# include <string>
# include <variant>
# include <vector>
# include <map>

namespace sihd::util
{

typedef std::variant<int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
int64_t, uint64_t, float, double, bool, std::string>    variant_datatype;

enum Datatypes {
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

class Datatype
{
    private:
        Datatype() {};
        ~Datatype() {};

        static std::vector<std::string>             v_datatype_to_string;
        static std::map<std::string, Datatypes>    m_datatype_from_string;

    public:
        static std::string  datatype_to_string(Datatypes type);
        static Datatypes   string_to_datatype(const std::string & type);

        template <typename T>
        static Datatypes   type_to_datatype()
        {
            return OBJECT;
        }   
};

template <>
Datatypes    Datatype::type_to_datatype<int8_t>();
template <>
Datatypes    Datatype::type_to_datatype<uint8_t>();
template <>
Datatypes    Datatype::type_to_datatype<int16_t>();
template <>
Datatypes    Datatype::type_to_datatype<uint16_t>();
template <>
Datatypes    Datatype::type_to_datatype<int32_t>();
template <>
Datatypes    Datatype::type_to_datatype<uint32_t>();
template <>
Datatypes    Datatype::type_to_datatype<int64_t>();
template <>
Datatypes    Datatype::type_to_datatype<uint64_t>();
template <>
Datatypes    Datatype::type_to_datatype<float>();
template <>
Datatypes    Datatype::type_to_datatype<double>();
template <>
Datatypes    Datatype::type_to_datatype<bool>();
template <>
Datatypes    Datatype::type_to_datatype<std::string>();

}

#endif 