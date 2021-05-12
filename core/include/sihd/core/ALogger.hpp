#ifndef __SIHD_CORE_ALOGGER_HPP__
# define __SIHD_CORE_ALOGGER_HPP__

# include <sihd/core/LogInfo.hpp>
# include <sihd/core/ILoggerFilter.hpp>
# include <sihd/core/ALogFilterer.hpp>
# include <sstream>

namespace sihd::core
{

class ALogger:  public ALogFilterer
{
    public:
        ALogger() {};
        virtual ~ALogger() {};

        virtual void    log(const LogInfo & info, const char *msg) = 0;

    private:
};

}

#endif 