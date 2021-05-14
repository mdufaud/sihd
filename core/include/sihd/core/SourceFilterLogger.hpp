#ifndef __SIHD_CORE_SOURCEFILTERLOGGER_HPP__
# define __SIHD_CORE_SOURCEFILTERLOGGER_HPP__

# include <sihd/core/ILoggerFilter.hpp>

namespace sihd::core
{

class SourceFilterLogger:   public ILoggerFilter
{
    public:
        SourceFilterLogger(const std::string & source, bool match_only = false);
        virtual ~SourceFilterLogger();

        virtual bool    filter(const LogInfo & info, const char *msg);

        std::string source;
        bool    match_only;
};

}

#endif 