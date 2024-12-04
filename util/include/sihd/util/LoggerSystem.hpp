#ifndef __SIHD_UTIL_LOGGERSYSTEM_HPP__
#define __SIHD_UTIL_LOGGERSYSTEM_HPP__

#include <sihd/util/ALogger.hpp>
#include <sihd/util/platform.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <syslog.h>
#else
# include <handleapi.h>
#endif

namespace sihd::util
{

class LoggerSystem: public ALogger
{
    public:
#if !defined(__SIHD_WINDOWS__)
        LoggerSystem(std::string_view progname, int facility = LOG_USER, int options = LOG_NDELAY | LOG_PID);
#else
        LoggerSystem(std::string_view progname, int facility = 0, int options = 0);
#endif
        ~LoggerSystem();

        void log(const LogInfo & info, std::string_view msg) override;

    protected:

    private:
#if defined(__SIHD_WINDOWS__)
        HANDLE _handle;
#endif
};

} // namespace sihd::util

#endif