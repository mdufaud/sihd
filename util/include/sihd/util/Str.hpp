#ifndef __SIHD_UTIL_STR_HPP__
# define __SIHD_UTIL_STR_HPP__

# include <iostream>
# include <vector>
# include <string>
# include <sstream>
# include <mutex>
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
        static size_t           hexdump_cols;

        static std::vector<std::string>    split(const std::string & s, const char *delimiter);
        static std::string      demangle(const char *name);
        static std::string      join(const std::vector<std::string> & join_lst, const std::string & join_with = "");
        static std::string      format(const char *format, ...);
        static std::string      trim(const std::string & s);
        static std::string &    to_upper(std::string & s);
        static std::string &    to_lower(std::string & s);
        static std::string      replace(const std::string & s, const std::string & from, const std::string & to);

        static std::string      addr_to_string(void *addr, size_t padding = 0);
        static std::string      num_to_string(size_t num, size_t base);
        static char             num_to_char(size_t num);
        static std::string      hexdump(void *mem, size_t size, char delim = ' ');
        // formatted hexdump
        static std::string      full_hexdump(void *mem, size_t size);

        static bool is_printable(int c);
        static bool starts_with(const std::string & s, const std::string & start);
        static bool ends_with(const std::string & s, const std::string & end);
};

}

#endif 