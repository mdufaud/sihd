#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerSystem.hpp>
#include <sihd/util/os.hpp>

#if defined(__SIHD_WINDOWS__)
# include <fmt/format.h>
# include <stdexcept>
# include <windows.h>
#endif

namespace sihd::util
{

SIHD_LOGGER;

LoggerSystem::LoggerSystem(std::string_view progname, int options, int facility)
{
#if !defined(__SIHD_WINDOWS__)
    openlog(progname.data(), options, facility);
#else
    _handle = RegisterEventSource(NULL, progname.data());
    if (_handle == nullptr)
    {
        throw std::runtime_error(fmt::format("Syslogger could not RegisterEventSource: {}", os::last_error_str()));
    }
    (void)options;
    (void)facility;
#endif
}

LoggerSystem::~LoggerSystem()
{
#if !defined(__SIHD_WINDOWS__)
    closelog();
#else
    DeregisterEventSource(_handle);
#endif
}

void LoggerSystem::log(const LogInfo & info, std::string_view msg)
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
    WORD type;
    switch (info.level)
    {
        case LogLevel::emergency:
        case LogLevel::alert:
        case LogLevel::critical:
        case LogLevel::error:
            type = EVENTLOG_ERROR_TYPE;
            break;
        case LogLevel::warning:
            type = EVENTLOG_WARNING_TYPE;
            break;
        case LogLevel::notice:
        case LogLevel::info:
        case LogLevel::debug:
            type = EVENTLOG_INFORMATION_TYPE;
            break;
        default:
            type = EVENTLOG_AUDIT_SUCCESS;
    }
    WORD category = static_cast<WORD>(info.level);
    const std::string report_str = fmt::format("{0}.{1}\t[{2}]\t{3:<9} {4}\t{5}\n",
                                               info.timespec.tv_sec,
                                               info.timespec.tv_nsec,
                                               info.thread_name.data(),
                                               info.strlevel,
                                               info.source.data(),
                                               msg.data());
    if (!ReportEvent(_handle,                        // Event log handle
                     type,                           // Event type
                     category,                       // Event category
                     0x1000,                         // Event identifier
                     NULL,                           // No security identifier
                     1,                              // Number of strings
                     0,                              // No binary data
                     (LPCSTR *)(report_str.c_str()), // Array of strings
                     NULL))                          // No binary data
    {
        throw std::runtime_error(fmt::format("Syslogger could not register event: {}", os::last_error_str()));
    }
#endif
}

} // namespace sihd::util
