#ifndef __SIHD_CURSES_CURSES_HPP__
# define __SIHD_CURSES_CURSES_HPP__

# include <mutex>

# include <sihd/util/platform.hpp>

# if !defined(__SIHD_WINDOWS__)
struct _win_st;
typedef struct _win_st WINDOW;
# else
struct _win;
typedef struct _win WINDOW;
# endif

namespace sihd::curses
{

class Curses
{
    public:
        Curses(bool raw = false);
        virtual ~Curses();

        static bool refresh();
        static bool erase();
        static bool clear();

        static bool start(bool raw);
        static bool stop();
        static bool is_started();

    protected:

    private:
        inline static bool _started = false;
        inline static std::mutex _mutex = {};
};

}

#endif