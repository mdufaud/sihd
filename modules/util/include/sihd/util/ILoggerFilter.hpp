#ifndef __SIHD_UTIL_ILOGGERFILTER_HPP__
#define __SIHD_UTIL_ILOGGERFILTER_HPP__

#include <sihd/util/LogInfo.hpp>

namespace sihd::util
{

class ILoggerFilter
{
    public:
        virtual ~ILoggerFilter() = default;
        virtual bool filter(const LogInfo & info, std::string_view msg) = 0;
};

} // namespace sihd::util

#endif