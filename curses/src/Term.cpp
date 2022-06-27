# include <curses.h>

#include <sihd/curses/Term.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::curses
{

SIHD_NEW_LOGGER("sihd::curses");

Term::Term(bool raw)
{
    Term::start(raw);
}

Term::~Term()
{
    Term::stop();
}

bool    Term::start(bool raw)
{
    std::lock_guard l(Term::_mutex);
    if (Term::_started)
        return true;
    if (::initscr() == nullptr)
    {
        SIHD_LOG(error, "Term: initscr() error");
        return false;
    }
    Term::_started = true;
    // no char print
    if (::noecho() != OK)
    {
        SIHD_LOG(error, "Term: noecho() error");
        return false;
    }
    // no cursor invisible
    if (::curs_set(0) != OK)
    {
        SIHD_LOG(error, "Term: curs_set(0) error");
        return false;
    }
    // curses will be able to make better use of the line-feed capability,
    // resulting in faster cursor motion and able to detect the return key.
    if (::nonl() != OK)
    {
        SIHD_LOG(error, "Term: nonl() error");
        return false;
    }
    if (::has_colors())
    {
        if (::start_color() != OK)
        {
            SIHD_LOG(error, "Term: start_color() error");
            return false;
        }
    }
    // get any char + signals (ctrl+c ctrl+z)
    if (raw)
    {
        if (::raw() != OK)
        {
            SIHD_LOG(error, "Term: raw() error");
            return false;
        }
    }
    else
    {
        if (::cbreak() != OK)
        {
            SIHD_LOG(error, "Term: cbreak() error");
            return false;
        }
    }
    if (::refresh() != OK)
    {
        SIHD_LOG(error, "Term: refresh() error");
        return false;
    }
    return true;
}

bool    Term::is_started()
{
    std::lock_guard l(Term::_mutex);
    return Term::_started;
}

bool    Term::stop()
{
    std::lock_guard l(Term::_mutex);
    if (Term::_started == false)
        return true;
    standend();
    ::refresh();
    ::curs_set(1);
    if (::endwin() != OK)
    {
        SIHD_LOG(error, "Term: endwin() error");
        return false;
    }
    Term::_started = false;
    return true;
}

bool    Term::refresh()
{
    return wrefresh(stdscr) == OK;
}

bool    Term::erase()
{
    return werase(stdscr) == OK;
}

bool    Term::clear()
{
    return wclear(stdscr) == OK;
}

}