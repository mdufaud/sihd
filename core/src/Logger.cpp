#include <sihd/core/Logger.hpp>

namespace sihd::core
{

Logger::Logger(const std::string & name): name(name)
{
}

Logger::~Logger()
{
}

void    Logger::debug(const char *msg)
{
    this->log(LogLevel::debug, msg);
}

void    Logger::info(const char *msg)
{
    this->log(LogLevel::info, msg);
}

void    Logger::warning(const char *msg)
{
    this->log(LogLevel::warning, msg);
}

void    Logger::error(const char *msg)
{
    this->log(LogLevel::error, msg);
}

void    Logger::critical(const char *msg)
{
    this->log(LogLevel::critical, msg);
}

void    Logger::log(LogLevel level, const char *msg)
{
    LoggerManager::get()->log(name.c_str(), level, msg);
}

}