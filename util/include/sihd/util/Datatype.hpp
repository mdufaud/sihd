#ifndef __SIHD_UTIL_DATATYPE_HPP__
# define __SIHD_UTIL_DATATYPE_HPP__

# include <string>
# include <variant>
# include <vector>
# include <map>

namespace sihd::util
{

typedef std::variant<bool, char, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
int64_t, uint64_t, float, double, std::string> variant_datatype;

enum Type {
    DNONE,
    DBOOL,
    DCHAR,
    DBYTE,
    DUBYTE,
    DSHORT,
    DUSHORT,
    DINT,
    DUINT,
    DLONG,
    DULONG,
    DFLOAT,
    DDOUBLE,
    DSTRING,
    DOBJECT,
};

class Datatype
{
    private:
        Datatype() {};
        ~Datatype() {};

        static std::vector<std::string> dt_to_str;
        static std::map<std::string, Type> dt_from_str;

    public:
        static size_t datatype_size(Type type);
        static std::string datatype_to_string(Type type);
        static Type string_to_datatype(const std::string & type);

        template <typename T>
        static Type type_to_datatype()
        {
            return DOBJECT;
        }

        template <typename T>
        static std::string type_to_string()
        {
            return datatype_to_string(type_to_datatype<T>());
        }
};

template <>
Type    Datatype::type_to_datatype<bool>();
template <>
Type    Datatype::type_to_datatype<char>();
template <>
Type    Datatype::type_to_datatype<int8_t>();
template <>
Type    Datatype::type_to_datatype<uint8_t>();
template <>
Type    Datatype::type_to_datatype<int16_t>();
template <>
Type    Datatype::type_to_datatype<uint16_t>();
template <>
Type    Datatype::type_to_datatype<int32_t>();
template <>
Type    Datatype::type_to_datatype<uint32_t>();
template <>
Type    Datatype::type_to_datatype<int64_t>();
template <>
Type    Datatype::type_to_datatype<uint64_t>();
template <>
Type    Datatype::type_to_datatype<float>();
template <>
Type    Datatype::type_to_datatype<double>();
template <>
Type    Datatype::type_to_datatype<std::string>();

}

#endif