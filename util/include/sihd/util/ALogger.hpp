#ifndef __SIHD_UTIL_ALOGGER_HPP__
# define __SIHD_UTIL_ALOGGER_HPP__

# include <sihd/util/LogInfo.hpp>
# include <sihd/util/ILoggerFilter.hpp>
# include <sihd/util/ALogFilterer.hpp>
# include <sstream>

namespace sihd::util
{

class ALogger:  public ALogFilterer
{
    public:
        ALogger() {};
        virtual ~ALogger() {};

        virtual void log(const LogInfo & info, const char *msg) = 0;

    private:
};

}

#endif 