#include <sihd/curses/CursesLogger.hpp>
#include <sihd/util/Logger.hpp>

#include <curses.h>

namespace sihd::curses
{

CursesLogger::CursesLogger(WINDOW *win):
    _win_ptr(win == nullptr ? stdscr : win)
{
}

CursesLogger::~CursesLogger()
{
}

void    CursesLogger::log(const sihd::util::LogInfo & info, std::string_view msg)
{
#if defined(__SIHD_WINDOWS__)
    wprintw(_win_ptr, "%lld.%09ld\t[%s]\t%s\t%s\t%s\n",
# else
    wprintw(_win_ptr, "%ld.%09ld\t[%s]\t%s\t%s\t%s\n",
#endif
            info.timestamp.tv_sec, info.timestamp.tv_nsec,
            info.thread_name.data(),
            info.strlevel, info.source.data(), msg.data());
}

}