#include <windows.h>

#include <fmt/format.h>

#include <sihd/sys/LoggerSystem.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

struct LoggerSystem::Impl
{
        HANDLE handle = nullptr;
};

LoggerSystem::~LoggerSystem()
{
    if (_impl != nullptr)
    {
        if (_impl->handle != nullptr)
            DeregisterEventSource(_impl->handle);
        delete _impl;
    }
}

std::expected<void, Error> LoggerSystem::_open_source()
{
    _impl = new Impl();
    _impl->handle = RegisterEventSource(NULL, _progname.c_str());
    if (_impl->handle == nullptr)
    {
        // the ctor throws on this return: the destructor never runs
        delete _impl;
        _impl = nullptr;
        return std::unexpected(
            Error(ErrorCode::io_error, "cannot RegisterEventSource '{}': {}", _progname, os::last_error_str()));
    }
    return {};
}

void LoggerSystem::log(const LogInfo & info, std::string_view msg)
{
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
    const std::string report_str = info.format(msg);
    // a failing ReportEvent (a full event log) must log nothing back: the manager
    // would re-enter this sink and recurse
    LPCSTR strings[] = {report_str.c_str()};
    (void)ReportEvent(_impl->handle, // Event log handle
                      type,          // Event type
                      category,      // Event category
                      0x1000,        // Event identifier
                      NULL,          // No security identifier
                      1,             // Number of strings
                      0,             // No binary data
                      strings,       // Array of strings
                      NULL);         // No binary data
}

} // namespace sihd::sys
