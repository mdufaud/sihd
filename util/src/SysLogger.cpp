#include <sihd/util/SysLogger.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_LOGGER;

SysLogger::SysLogger(const std::string & progname, int options, int facility)
{
#if !defined(__SIHD_WINDOWS__)
    openlog(progname.c_str(), options, facility);
#else
    (void)progname; (void)options; (void)facility;
#endif
}

SysLogger::~SysLogger()
{
#if !defined(__SIHD_WINDOWS__)
    closelog();
#endif
}

void    SysLogger::log(const LogInfo & info, const char *msg)
{
#if !defined(__SIHD_WINDOWS__)
    int prio;

    switch (info.level)
    {
        case LogLevel::debug:
            prio = LOG_DEBUG;
            break ;
        case LogLevel::info:
            prio = LOG_INFO;
            break ;
        case LogLevel::warning:
            prio = LOG_WARNING;
            break ;
        case LogLevel::error:
            prio = LOG_ERR;
            break ;
        case LogLevel::critical:
            prio = LOG_CRIT;
            break ;
        default:
            prio = LOG_NOTICE;
    }
    syslog(prio, "%ld.%09ld\t[%s]\t%s\t%s\t%s\n",
            info.timestamp.tv_sec, info.timestamp.tv_nsec,
            info.thread_name.data(),
            info.level_str, info.source.data(), msg);
#else
    (void)info; (void)msg;
#endif
}

}