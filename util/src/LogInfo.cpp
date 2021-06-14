#include <sihd/util/LogInfo.hpp>
#include <array>

namespace sihd::util
{

LogInfo::LogInfo(const std::string & src, LogLevel lvl): source(src.c_str()), level(lvl)
{
    thread_id = std::this_thread::get_id();
    thread_id_str = sihd::util::thread::id_str(thread_id);
    thread_name = sihd::util::thread::get_name();
    level_str = this->get_level();
    ::clock_gettime(CLOCK_REALTIME, &timestamp);
}

LogInfo::~LogInfo()
{
}

std::array<std::string, (int)LogLevel::critical + 1> _levels_to_str_array = {
    "NOLEVEL", "DEBUG", "INFO", "WARNING", "ERROR", "CRIT"
};

std::map<std::string, LogLevel> _str_to_levels_map = {
    {"DEBUG", LogLevel::debug},
    {"INFO", LogLevel::info},
    {"WARNING", LogLevel::warning},
    {"ERROR", LogLevel::error},
    {"CRITICAL", LogLevel::critical}
};

const std::string & LogInfo::get_level() const
{
    return _levels_to_str_array[(int)this->level];
}

const std::string & LogInfo::get_level(LogLevel level)
{
    return _levels_to_str_array[level];
}

LogLevel    LogInfo::string_to_level(const std::string & level)
{
    if (_str_to_levels_map.find(level) != _str_to_levels_map.end())
        return _str_to_levels_map[level];
    return LogLevel::none;
}

}