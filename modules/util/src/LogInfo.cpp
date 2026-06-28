#include <array>
#include <map>

#include <sihd/util/LogInfo.hpp>

namespace sihd::util
{

LogInfo::LogInfo(const std::string & src, LogLevel lvl): source(src), level(lvl)
{
    this->thread_id = thread::id();
    this->thread_id_str = thread::id_str(thread_id);
    this->thread_name = thread::name();
    this->strlevel = this->level_str(this->level);
    ::clock_gettime(CLOCK_REALTIME, &timespec);
}

LogInfo::~LogInfo() = default;

Timestamp LogInfo::timestamp() const
{
    return Timestamp(this->timespec);
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
    static std::map<std::string_view, LogLevel> log_to_str = {{"EMERGENCY", LogLevel::emergency},
                                                              {"ALERT", LogLevel::alert},
                                                              {"CRITICAL", LogLevel::critical},
                                                              {"ERROR", LogLevel::error},
                                                              {"WARNING", LogLevel::warning},
                                                              {"NOTICE", LogLevel::notice},
                                                              {"INFO", LogLevel::info},
                                                              {"DEBUG", LogLevel::debug}};
    auto it = log_to_str.find(level);
    if (it == log_to_str.end())
        return LogLevel::none;
    return it->second;
}

} // namespace sihd::util
