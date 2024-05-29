#ifndef __SIHD_UTIL_LEVELFILTERLOGGER_HPP__
#define __SIHD_UTIL_LEVELFILTERLOGGER_HPP__

#include <sihd/util/ILoggerFilter.hpp>

namespace sihd::util
{

class LevelFilterLogger: public ILoggerFilter
{
    public:
        LevelFilterLogger(LogLevel min, bool match = false);
        LevelFilterLogger(const std::string & level, bool match = false);
        virtual ~LevelFilterLogger();

        virtual bool filter(const LogInfo & info, std::string_view msg);

        LogLevel level;
        bool match;
};

} // namespace sihd::util

#endif