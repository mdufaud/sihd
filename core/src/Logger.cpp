#include <sihd/core/Logger.hpp>

namespace sihd::core
{

Logger::Logger(const std::string & name): name(name)
{
}

Logger::~Logger()
{
}

void    Logger::log(LogLevel level, const char *msg)
{
    LoggerManager::get()->log(name.c_str(), level, msg);
}

}