#include <sihd/curses/Term.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::curses
{

SIHD_NEW_LOGGER("sihd::curses");

Term::Term()
{
    initscr();
    // no char print
    noecho();
    // get any char + signals (ctrl+c ctrl+z)
    //raw();
    cbreak();
    // get keys like F1
    keypad(stdscr, true);
    // curses will be able to make better use of the line-feed capability,
    // resulting in faster cursor motion and able to detect the return key.
    // nonl();
    if (has_colors())
    {
        start_color();
    }
    refresh();
}

Term::~Term()
{
    endwin();
}

}