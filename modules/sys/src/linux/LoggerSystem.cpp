#include <syslog.h>

#include <mutex>

#include <sihd/sys/LoggerSystem.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

namespace
{

std::mutex g_syslog_mutex;
int g_instances = 0;
std::string g_ident;

} // namespace

struct LoggerSystem::Impl
{
};

LoggerSystem::~LoggerSystem()
{
    std::lock_guard<std::mutex> lock(g_syslog_mutex);
    if (--g_instances == 0)
        closelog();
}

std::expected<void, Error> LoggerSystem::_open_source()
{
    std::lock_guard<std::mutex> lock(g_syslog_mutex);
    if (g_instances > 0)
    {
        if (g_ident != _progname)
            SIHD_LOG(warning, "LoggerSystem: syslog ident '{}' already set, messages will use it", g_ident);
    }
    else
    {
        // libc retains the ident pointer: it must live longer than any instance
        g_ident = _progname;
        openlog(g_ident.c_str(), _options, _facility);
    }
    ++g_instances;
    return {};
}

void LoggerSystem::log(const LogInfo & info, std::string_view msg)
{
    // loglevel is done same as syslog, the facility rides the priority
    syslog(static_cast<int>(info.level) | _facility, "%s", info.format(msg).c_str());
}

} // namespace sihd::sys
