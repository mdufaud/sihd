#ifndef __SIHD_UTIL_ATEXIT_HPP__
# define __SIHD_UTIL_ATEXIT_HPP__

# include <sihd/util/IRunnable.hpp>
# include <mutex>
# include <list>
# include <cstdlib>

namespace sihd::util
{

class AtExit
{
    private:
        AtExit() {};
        ~AtExit() {};

        static bool installed;
        static std::mutex runnable_mutex;
        static std::list<IRunnable *> runnables;

        // actual goodbye callback - calls handlers post-exit and deletes them
        static void exit_callback();

    public:
        // adds handler to be run post-exit
        static void add_handler(IRunnable *ptr);
        // remove handler and does NOT delete it
        static void remove_handler(IRunnable *ptr);
        // remove all handlers and deletes them
        static void clear_handlers();
        // permits exit callback post-exit
        static bool install();


};

}

#endif