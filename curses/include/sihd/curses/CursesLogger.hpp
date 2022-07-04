#ifndef __SIHD_CURSES_CURSESLOGGER_HPP__
# define __SIHD_CURSES_CURSESLOGGER_HPP__

# include <sihd/util/ALogger.hpp>
# include <sihd/curses/Curses.hpp>

namespace sihd::curses
{

class CursesLogger: public sihd::util::ALogger
{
    public:
        // takes stdscr by default if nullptr
        CursesLogger(WINDOW *win = nullptr);
        virtual ~CursesLogger();

        virtual void log(const sihd::util::LogInfo & info, std::string_view msg) override;

    protected:

    private:
        WINDOW *_win_ptr;
};

}

#endif