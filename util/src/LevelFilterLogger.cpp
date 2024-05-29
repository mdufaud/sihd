#include <sihd/util/LevelFilterLogger.hpp>

namespace sihd::util
{

LevelFilterLogger::LevelFilterLogger(LogLevel lvl, bool match): level(lvl), match(match) {}

LevelFilterLogger::LevelFilterLogger(const std::string & lvl, bool match): match(match)
{
    this->level = LogInfo::level_from_str(lvl);
}

LevelFilterLogger::~LevelFilterLogger() {}

bool LevelFilterLogger::filter(const LogInfo & info, std::string_view msg)
{
    (void)msg;
    // filter exact loglevel or lower loglevels
    return this->match ? info.level == this->level : this->level < info.level;
}

} // namespace sihd::util