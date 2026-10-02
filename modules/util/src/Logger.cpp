#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_NEW_LOGGER("sihd::util");

Logger::Logger(const std::string & name): name(name) {}

Logger::~Logger() = default;

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

void log_unexpected_error(Logger & logger, const Error & err, const std::source_location & loc)
{
    std::string_view file = loc.file_name();
    if (file.empty())
    {
        logger.log(LogLevel::error, err.message);
        return;
    }
    file.remove_prefix(file.find_last_of("/\\") + 1);
    const size_t dot = file.find_last_of('.');
    if (dot != std::string_view::npos)
        file.remove_suffix(file.size() - dot);
    logger.log(LogLevel::error, "{}:{}: {}", file, loc.line(), err.message);
}

} // namespace sihd::util
