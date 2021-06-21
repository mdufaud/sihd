#ifndef __SIHD_UTIL_OS_HPP__
# define __SIHD_UTIL_OS_HPP__

# include <string>
# include <dlfcn.h>

namespace sihd::util::os
{

void    *load_lib(std::string lib_name);

}

#endif 