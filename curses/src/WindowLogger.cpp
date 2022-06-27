#include <sihd/curses/WindowLogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

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
    sihd::util::LoggerManager::remove(_logger_ptr);
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
    auto [yb, xb] = _parent_ptr->win_cursor_yx();
    _parent_ptr->win_write("{}.{:09}\t[{}]\t{}\t{}\t{}\n",
                            info.timestamp.tv_sec, info.timestamp.tv_nsec,
                            info.thread_name,
                            info.strlevel, info.source, msg, yb, xb, _parent_ptr->gui_conf().padding.left);
    // auto [ya, xa] = _parent_ptr->win_cursor_yx();
    // _parent_ptr->win_write("{}\t{}\t{}\n",
    //                         info.strlevel, info.source, msg);
}

}