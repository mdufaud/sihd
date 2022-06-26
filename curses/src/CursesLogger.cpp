#include <sihd/curses/CursesLogger.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::curses
{

CursesLogger::CursesLogger(WINDOW *win): _win_ptr(win)
{
}

CursesLogger::~CursesLogger()
{
}

void    CursesLogger::log(const sihd::util::LogInfo & info, std::string_view msg)
{
#if defined(__SIHD_WINDOWS__)
    wprintw(_win_ptr, "%lld.%09ld\t%s[%s]\t%s\t%s\t%s\n",
# else
    wprintw(_win_ptr, "%ld.%09ld\t%s[%s]\t%s\t%s\t%s\n",
#endif
            info.timestamp.tv_sec, info.timestamp.tv_nsec,
            info.thread_id_str.c_str(),
            info.thread_name.data(),
            info.strlevel, info.source.data(), msg.data());
    wrefresh(_win_ptr);
}

}