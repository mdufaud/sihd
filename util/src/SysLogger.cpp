#include <sihd/util/Logger.hpp>
#include <sihd/util/SysLogger.hpp>

namespace sihd::util
{

SIHD_LOGGER;

SysLogger::SysLogger(std::string_view progname, int options, int facility)
{
#if !defined(__SIHD_WINDOWS__)
    openlog(progname.data(), options, facility);
#else
    (void)progname;
    (void)options;
    (void)facility;
#endif
}

SysLogger::~SysLogger()
{
#if !defined(__SIHD_WINDOWS__)
    closelog();
#endif
}

void SysLogger::log(const LogInfo & info, std::string_view msg)
{
#if !defined(__SIHD_WINDOWS__)
    // loglevel is done same as syslog
    syslog(static_cast<int>(info.level),
           "%ld.%09ld\t[%s]\t%s\t%s\t%s\n",
           info.timespec.tv_sec,
           info.timespec.tv_nsec,
           info.thread_name.data(),
           info.strlevel,
           info.source.data(),
           msg.data());
#else
# pragma message("TODO")
    (void)info;
    (void)msg;
#endif
}

} // namespace sihd::util
