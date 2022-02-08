#include <sihd/util/LogInfo.hpp>
#include <array>

namespace sihd::util
{

LogInfo::LogInfo(const std::string & src, LogLevel lvl): source(src), level(lvl)
{
    thread_id = Thread::id();
    thread_id_str = Thread::id_str(thread_id);
    thread_name = Thread::get_name();
    level_str = this->get_level(this->level);
    ::clock_gettime(CLOCK_REALTIME, &timestamp);
}

LogInfo::~LogInfo()
{
}

const char  *LogInfo::get_level(LogLevel level)
{
    switch (level)
    {
        case LogLevel::debug:
            return "DEBUG";
        case LogLevel::info:
            return "INFO";
        case LogLevel::warning:
            return "WARNING";
        case LogLevel::error:
            return "ERROR";
        case LogLevel::critical:
            return "CRITICAL";
        default:
            return "NONE";
    }
}

LogLevel    LogInfo::string_to_level(const std::string & level)
{
    if (level == "INFO")
        return LogLevel::info;
    else if (level == "ERROR")
        return LogLevel::error;
    else if (level == "WARNING")
        return LogLevel::warning;
    else if (level == "DEBUG")
        return LogLevel::debug;
    else if (level == "CRITICAL")
        return LogLevel::critical;
    return LogLevel::none;
}

}