#ifndef __SIHD_UTIL_LOGGERFILTERSOURCE_HPP__
#define __SIHD_UTIL_LOGGERFILTERSOURCE_HPP__

#include <sihd/util/ILoggerFilter.hpp>

namespace sihd::util
{

class LoggerFilterSource: public ILoggerFilter
{
    public:
        LoggerFilterSource(const std::string & source, bool match_only = false);
        ~LoggerFilterSource();

        bool filter(const LogInfo & info, std::string_view msg);

        std::string source;
        bool match_only;
};

} // namespace sihd::util

#endif