#ifndef __SIHD_UTIL_ILOGGERFILTER_HPP__
#define __SIHD_UTIL_ILOGGERFILTER_HPP__

#include <sihd/util/LogInfo.hpp>

namespace sihd::util
{

class ILoggerFilter
{
    public:
        virtual ~ILoggerFilter() = default;
        // phase 1: drop conditions computable without the formatted message
        virtual bool filter(const LogInfo & info) = 0;
        // phase 2: drop conditions that need the formatted message
        virtual bool filter(const LogInfo & info, std::string_view msg) = 0;
};

} // namespace sihd::util

#endif
