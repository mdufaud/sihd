#include <sihd/util/LoggerFilterLevel.hpp>

namespace sihd::util
{

LoggerFilterLevel::LoggerFilterLevel(LogLevel lvl, bool match): level(lvl), match(match) {}

LoggerFilterLevel::LoggerFilterLevel(const std::string & lvl, bool match): match(match)
{
    this->level = LogInfo::level_from_str(lvl);
}

LoggerFilterLevel::~LoggerFilterLevel() {}

bool LoggerFilterLevel::filter(const LogInfo & info, std::string_view msg)
{
    (void)msg;
    // filter exact loglevel or lower loglevels
    return this->match ? info.level == this->level : this->level < info.level;
}

} // namespace sihd::util