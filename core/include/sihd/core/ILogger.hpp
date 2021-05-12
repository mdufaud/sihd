#ifndef __SIHD_CORE_ILOGGER_HPP__
# define __SIHD_CORE_ILOGGER_HPP__

# include <sihd/core/LogInfo.hpp>
# include <sstream>

namespace sihd::core
{

class ILogger
{
    public:
        ILogger() {};
        virtual ~ILogger() {};

        virtual void    log(const LogInfo & info, const char *msg) = 0;

    private:
};

}

#endif 