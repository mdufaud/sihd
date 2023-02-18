#ifndef __SIHD_ZIP_ZIPUTILS_HPP__
# define __SIHD_ZIP_ZIPUTILS_HPP__

# include <zip.h>

# include <string>

namespace sihd::zip::utils
{

std::string get_error(int code);
std::string get_error(zip_t *ptr);

}

#endif