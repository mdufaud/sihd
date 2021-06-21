#ifndef __SIHD_UTIL_PATH_HPP__
# define __SIHD_UTIL_PATH_HPP__

# include <sihd/util/str.hpp>
# include <map>
# include <mutex>
# include <experimental/filesystem>

namespace sihd::util::path
{

void    clear_all();
void    clear(const std::string & uri);
void    set(const std::string & uri, const std::string & path);
std::string get(const std::string & path);
std::string get(const std::string & uri, const std::string & path);
std::string get_from(const std::string & from, const std::string & path);
std::string find(const std::string & path);

}

#endif 