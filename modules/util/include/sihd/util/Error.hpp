#ifndef __SIHD_UTIL_ERROR_HPP__
#define __SIHD_UTIL_ERROR_HPP__

#include <string>

namespace sihd::util
{

struct Error
{
        int code = 0;
        std::string message;
};

} // namespace sihd::util

#endif
