#ifndef __SIHD_UTIL_STR_HPP__
# define __SIHD_UTIL_STR_HPP__

# include <iostream>
# include <vector>
# include <string>
# include <sstream>
# include <mutex>

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
        static std::vector<std::string>    split(const std::string & s, const char *delimiter);
        static std::string     demangle(const char *name);
        static std::string     join(const std::vector<std::string> & join_lst, const std::string & join_with = "");
        static std::string     format(const char *format, ...);
        static std::string     trim(const std::string & s);
        static std::string &   to_upper(std::string & s);
        static std::string &   to_lower(std::string & s);
        static std::string     replace(const std::string & s, const std::string & from, const std::string & to);

};

}

#endif 