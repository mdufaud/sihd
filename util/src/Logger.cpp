#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_NEW_LOGGER("sihd::util");

Logger::Logger(const std::string & name): name(name) {}

Logger::~Logger() {}

void Logger::emergency(std::string_view msg)
{
    this->log(LogLevel::emergency, msg);
}

void Logger::alert(std::string_view msg)
{
    this->log(LogLevel::alert, msg);
}

void Logger::critical(std::string_view msg)
{
    this->log(LogLevel::critical, msg);
}

void Logger::error(std::string_view msg)
{
    this->log(LogLevel::error, msg);
}

void Logger::warning(std::string_view msg)
{
    this->log(LogLevel::warning, msg);
}

void Logger::notice(std::string_view msg)
{
    this->log(LogLevel::notice, msg);
}

void Logger::info(std::string_view msg)
{
    this->log(LogLevel::info, msg);
}

void Logger::debug(std::string_view msg)
{
    this->log(LogLevel::debug, msg);
}

void Logger::log(LogLevel level, std::string_view msg)
{
    LoggerManager::log(name, level, msg);
}

} // namespace sihd::util