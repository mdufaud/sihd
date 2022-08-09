#include <sihd/curses/WindowLogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Term.hpp>

namespace sihd::curses
{

SIHD_UTIL_REGISTER_FACTORY(WindowLogger)

SIHD_LOGGER;

WindowLogger::WindowLogger(const std::string & name, sihd::util::Node *parent):
    Window(name, parent), _logger_ptr(new Logger(this))
{
    sihd::util::LoggerManager::add(_logger_ptr);
}

WindowLogger::~WindowLogger()
{
    sihd::util::LoggerManager::rm(_logger_ptr);
}

WindowLogger::Logger::Logger(Window *parent): _parent_ptr(parent)
{
}

WindowLogger::Logger::~Logger()
{
}

void    WindowLogger::Logger::log(const sihd::util::LogInfo & info, std::string_view msg)
{
    if (_parent_ptr->is_window_init() == false)
        return ;
    const char *level;

    switch (info.level)
    {
        case sihd::util::LogLevel::emergency:
            level = "EMER";
            break ;
        case sihd::util::LogLevel::alert:
            level = "A";
            break ;
        case sihd::util::LogLevel::critical:
            level = "C";
            break ;
        case sihd::util::LogLevel::error:
            level = "E";
            break ;
        case sihd::util::LogLevel::warning:
            level = "W";
            break ;
        case sihd::util::LogLevel::notice:
            level = "N";
            break ;
        case sihd::util::LogLevel::info:
            level = "I";
            break ;
        case sihd::util::LogLevel::debug:
            level = "D";
            break ;
        default:
            level = "-";
    }
    _parent_ptr->win_write("{} [{}] <{}> {}\n", level, info.thread_name.data(), info.source.data(), msg.data());
}

}