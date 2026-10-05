#include <syslog.h>

#include <sihd/sys/LoggerSystem.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

struct LoggerSystem::Impl
{
};

LoggerSystem::LoggerSystem(std::string_view progname, int facility, int options): _impl(std::make_unique<Impl>())
{
    openlog(progname.data(), options, facility);
}

LoggerSystem::~LoggerSystem()
{
    closelog();
}

void LoggerSystem::log(const LogInfo & info, std::string_view msg)
{
    // loglevel is done same as syslog
    syslog(static_cast<int>(info.level), "%s", info.format(msg).c_str());
}

} // namespace sihd::sys
