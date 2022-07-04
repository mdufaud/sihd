# include <curses.h>

#include <sihd/curses/Curses.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::curses
{

SIHD_NEW_LOGGER("sihd::curses");

Curses::Curses(bool raw)
{
    Curses::start(raw);
}

Curses::~Curses()
{
    Curses::stop();
}

bool    Curses::start(bool raw)
{
    std::lock_guard l(Curses::_mutex);
    if (Curses::_started)
        return true;
    if (::initscr() == nullptr)
    {
        SIHD_LOG(error, "Curses: initscr() error");
        return false;
    }
    Curses::_started = true;
    // no char print
    if (::noecho() != OK)
    {
        SIHD_LOG(error, "Curses: noecho() error");
        return false;
    }
    // no cursor invisible
    if (::curs_set(0) != OK)
    {
        SIHD_LOG(error, "Curses: curs_set(0) error");
        return false;
    }
    // curses will be able to make better use of the line-feed capability,
    // resulting in faster cursor motion and able to detect the return key.
    if (::nonl() != OK)
    {
        SIHD_LOG(error, "Curses: nonl() error");
        return false;
    }
    if (::has_colors())
    {
        if (::start_color() != OK)
        {
            SIHD_LOG(error, "Curses: start_color() error");
            return false;
        }
    }
    // get any char + signals (ctrl+c ctrl+z)
    if (raw)
    {
        if (::raw() != OK)
        {
            SIHD_LOG(error, "Curses: raw() error");
            return false;
        }
    }
    else
    {
        if (::cbreak() != OK)
        {
            SIHD_LOG(error, "Curses: cbreak() error");
            return false;
        }
    }
    if (::refresh() != OK)
    {
        SIHD_LOG(error, "Curses: refresh() error");
        return false;
    }
    return true;
}

bool    Curses::is_started()
{
    std::lock_guard l(Curses::_mutex);
    return Curses::_started;
}

bool    Curses::stop()
{
    std::lock_guard l(Curses::_mutex);
    if (Curses::_started == false)
        return true;
    standend();
    ::refresh();
    ::curs_set(1);
    if (::endwin() != OK)
    {
        SIHD_LOG(error, "Curses: endwin() error");
        return false;
    }
    Curses::_started = false;
    return true;
}

bool    Curses::refresh()
{
    return wrefresh(stdscr) == OK;
}

bool    Curses::erase()
{
    return werase(stdscr) == OK;
}

bool    Curses::clear()
{
    return wclear(stdscr) == OK;
}

}