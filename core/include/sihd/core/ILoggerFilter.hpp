#ifndef __SIHD_CORE_ILOGGERFILTER_HPP__
# define __SIHD_CORE_ILOGGERFILTER_HPP__

# include <sihd/core/LogInfo.hpp>

namespace sihd::core
{

class ILoggerFilter
{
    public:
        ILoggerFilter() {};
        virtual ~ILoggerFilter() {};

        virtual bool    filter(const LogInfo & info, const char *msg) = 0;

    private:
};

}

#endif 