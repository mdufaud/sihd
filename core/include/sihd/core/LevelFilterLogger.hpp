#ifndef __SIHD_CORE_LEVELFILTERLOGGER_HPP__
# define __SIHD_CORE_LEVELFILTERLOGGER_HPP__

# include <sihd/core/ILoggerFilter.hpp>

namespace sihd::core
{

class LevelFilterLogger:    public ILoggerFilter
{
    public:
        LevelFilterLogger(LogLevel min, bool match = false);
        LevelFilterLogger(const std::string & level, bool match = false);
        virtual ~LevelFilterLogger();

        virtual bool    filter(const LogInfo & info, const char *msg);

        LogLevel    level;
        bool    match;
    private:
};

}

#endif 