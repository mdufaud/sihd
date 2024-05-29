#ifndef __SIHD_UTIL_SYSLOGGER_HPP__
#define __SIHD_UTIL_SYSLOGGER_HPP__

#include <sihd/util/ALogger.hpp>
#include <sihd/util/platform.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <syslog.h>
#endif

namespace sihd::util
{

class SysLogger: public ALogger
{
    public:
#if !defined(__SIHD_WINDOWS__)
        SysLogger(std::string_view progname, int facility = LOG_USER, int options = LOG_NDELAY | LOG_PID);
#else
        SysLogger(std::string_view progname, int facility = 0, int options = 0);
#endif
        virtual ~SysLogger();

        virtual void log(const LogInfo & info, std::string_view msg) override;

    protected:

    private:
};

} // namespace sihd::util

#endif