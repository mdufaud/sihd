#ifndef __SIHD_UTIL_STR_HPP__
# define __SIHD_UTIL_STR_HPP__

# include <iostream>
# include <vector>
# include <string>
# include <sstream>
# include <mutex>
# include <map>
# include <optional>
# include <sihd/util/Num.hpp>

namespace sihd::util
{

class Str
{
    private:
        Str() {};
        ~Str() {};

        static const size_t buffer_size;
        static std::mutex   buffer_mutex;
        static char         buffer[];

    public:
        static size_t hexdump_cols;

        static std::vector<std::string> split(const std::string & s, const std::string & delimiter);
        static std::vector<std::string> split(const std::string & s, const char *delimiter);
        static std::string demangle(const char *name);
        static std::string join(const std::vector<std::string> & join_lst, const std::string & join_with = "");
        static std::string format(const char *format, ...);
        static std::string trim(const std::string & s);
        static std::string & to_upper(std::string & s);
        static std::string & to_lower(std::string & s);
        static std::string replace(const std::string & s, const std::string & from, const std::string & to);

        static std::string addr_to_string(void *addr, size_t padding = 0);
        static std::string num_to_string(size_t num, uint16_t base);
        static char num_to_char(size_t num);
        static std::string hexdump(void *mem, size_t size, char delim = ' ');
        // formatted hexdump
        static std::string hexdump_fmt(void *mem, size_t size);

        static bool is_digit(int c, uint16_t base = 10);
        static bool is_number(const std::string & s, uint16_t base = 10);
        static bool starts_with(const std::string & s, const std::string & start);
        static bool ends_with(const std::string & s, const std::string & end);

        static std::map<std::string, std::string> parse_configuration(const std::string & conf);

        static std::optional<long> to_long(const std::string & str, uint16_t base = 0);
        static std::optional<unsigned long> to_ulong(const std::string & str, uint16_t base = 0);
        static std::optional<double> to_double(const std::string & str);
};

}

#endif 