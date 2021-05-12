#include <sihd/core/LevelFilterLogger.hpp>

namespace sihd::core
{

LevelFilterLogger::LevelFilterLogger(LogLevel min): min_level(min)
{   
}

LevelFilterLogger::LevelFilterLogger(const std::string & level)
{
    this->min_level = LogInfo::string_to_level(level);
}

LevelFilterLogger::~LevelFilterLogger()
{
}

bool    LevelFilterLogger::filter(const LogInfo & info, const char *msg)
{
    (void)msg;
    return info.level < this->min_level;
}

}