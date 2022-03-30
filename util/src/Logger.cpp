#include <sihd/util/Logger.hpp>
#include <cstdarg>

namespace sihd::util
{

SIHD_NEW_LOGGER("sihd::util");

Logger::Logger(const std::string & name): name(name)
{
}

Logger::~Logger()
{
}

void    Logger::debug(std::string_view msg)
{
    this->log(LogLevel::debug, msg);
}

void    Logger::info(std::string_view msg)
{
    this->log(LogLevel::info, msg);
}

void    Logger::warning(std::string_view msg)
{
    this->log(LogLevel::warning, msg);
}

void    Logger::error(std::string_view msg)
{
    this->log(LogLevel::error, msg);
}

void    Logger::critical(std::string_view msg)
{
    this->log(LogLevel::critical, msg);
}

void    Logger::log(LogLevel level, std::string_view msg)
{
    LoggerManager::get()->log(name, level, msg);
}

}