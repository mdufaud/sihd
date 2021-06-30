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

enum e_datatype {
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
        static std::map<std::string, e_datatype>    m_datatype_from_string;

    public:
        static std::string  datatype_to_string(e_datatype type);
        static e_datatype   string_to_datatype(const std::string & type);

        template <typename T>
        static e_datatype   type_to_datatype()
        {
            return OBJECT;
        }   
};

}

#endif 