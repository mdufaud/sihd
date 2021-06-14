#ifndef __SIHD_UTIL_THREAD_HPP__
# define __SIHD_UTIL_THREAD_HPP__

# include <thread>
# include <string>
# include <map>

namespace sihd::util::thread
{

std::thread::id id();
std::string     id_str();
std::string     id_str(std::thread::id id);
void    set_name(const std::string & name);
void    del_name();
const std::string & get_name();

}

#endif 