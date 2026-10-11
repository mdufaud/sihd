#include <fmt/core.h>

#include <sihd/util/LogInfo.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

LogInfo::LogInfo(const std::string & src, LogLevel lvl): source(src), level(lvl)
{
    this->thread_id = thread::id();
    this->thread_id_str = thread::id_str(thread_id);
    this->thread_name = thread::name();
    this->strlevel = this->level_str(this->level);
    this->timespec = time::to_ts(Timestamp::now().get());
}

LogInfo::~LogInfo() = default;

Timestamp LogInfo::timestamp() const
{
    return Timestamp(this->timespec);
}

std::string LogInfo::format(std::string_view msg, bool print_thread_id) const
{
    if (print_thread_id)
    {
        return fmt::format("{0}.{1:09}\t{2}\t[{3}]\t{4:<9} {5}\t{6}\n",
                           timespec.tv_sec,
                           timespec.tv_nsec,
                           thread_id_str,
                           thread_name,
                           strlevel,
                           source,
                           msg);
    }
    return fmt::format("{0}.{1:09}\t[{2}]\t{3:<9} {4}\t{5}\n",
                       timespec.tv_sec,
                       timespec.tv_nsec,
                       thread_name,
                       strlevel,
                       source,
                       msg);
}

const char *LogInfo::level_str(LogLevel level)
{
    switch (level)
    {
        case LogLevel::emergency:
            return "EMERGENCY";
        case LogLevel::alert:
            return "ALERT";
        case LogLevel::critical:
            return "CRITICAL";
        case LogLevel::error:
            return "ERROR";
        case LogLevel::warning:
            return "WARNING";
        case LogLevel::notice:
            return "NOTICE";
        case LogLevel::info:
            return "INFO";
        case LogLevel::debug:
            return "DEBUG";
        default:
            return "NONE";
    }
}

LogLevel LogInfo::level_from_str(std::string_view level)
{
    if (level == "EMERGENCY")
        return LogLevel::emergency;
    if (level == "ALERT")
        return LogLevel::alert;
    if (level == "CRITICAL")
        return LogLevel::critical;
    if (level == "ERROR")
        return LogLevel::error;
    if (level == "WARNING")
        return LogLevel::warning;
    if (level == "NOTICE")
        return LogLevel::notice;
    if (level == "INFO")
        return LogLevel::info;
    if (level == "DEBUG")
        return LogLevel::debug;
    return LogLevel::none;
}

} // namespace sihd::util
