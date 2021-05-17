#ifndef __SIHD_CORE_STR_HPP__
# define __SIHD_CORE_STR_HPP__

# include <iostream>
# include <vector>
# include <string>
# include <sstream>

namespace sihd::core::str
{

std::vector<std::string>    split(const std::string & s, const char *delimiter);
std::string     demangle(const char *name);
std::string     join(const std::vector<std::string> & join_lst, const std::string & join_with = "");
std::string     format(const char *format, ...);
std::string     trim(const std::string & s);
std::string &   to_upper(std::string & s);
std::string &   to_lower(std::string & s);
std::string     replace(const std::string & s, const std::string & from, const std::string & to);

}

#endif 