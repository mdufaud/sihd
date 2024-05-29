#ifndef __SIHD_UTIL_SOURCEFILTERLOGGER_HPP__
#define __SIHD_UTIL_SOURCEFILTERLOGGER_HPP__

#include <sihd/util/ILoggerFilter.hpp>

namespace sihd::util
{

class SourceFilterLogger: public ILoggerFilter
{
    public:
        SourceFilterLogger(const std::string & source, bool match_only = false);
        virtual ~SourceFilterLogger();

        virtual bool filter(const LogInfo & info, std::string_view msg);

        std::string source;
        bool match_only;
};

} // namespace sihd::util

#endif