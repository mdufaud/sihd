#include <sihd/core/LogInfo.hpp>
#include <array>

namespace sihd::core
{

LogInfo::LogInfo(const std::string & src, LogLevel lvl): source(src.c_str()), level(lvl)
{
    thread_id = std::this_thread::get_id();
    thread_id_str = sihd::core::thread::id_str(thread_id);
    thread_name = sihd::core::thread::get_name();
    level_str = this->get_level();
    ::clock_gettime(CLOCK_REALTIME, &timestamp);
}

LogInfo::~LogInfo()
{
}

std::array<std::string, (int)LogLevel::critical + 1> _log_levels = {
    "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL" 
};

const std::string & LogInfo::get_level() const
{
    return _log_levels[(int)this->level];
}

}