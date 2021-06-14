#include <sihd/util/LevelFilterLogger.hpp>

namespace sihd::util
{

LevelFilterLogger::LevelFilterLogger(LogLevel lvl, bool match):
    level(lvl), match(match)
{   
}

LevelFilterLogger::LevelFilterLogger(const std::string & lvl, bool match):
    match(match)
{
    this->level = LogInfo::string_to_level(lvl);
}

LevelFilterLogger::~LevelFilterLogger()
{
}

bool    LevelFilterLogger::filter(const LogInfo & info, const char *msg)
{
    (void)msg;
    return this->match
        ? info.level == this->level
        : info.level < this->level;
}

}