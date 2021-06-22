#ifndef __SIHD_UTIL_ATEXIT_HPP__
# define __SIHD_UTIL_ATEXIT_HPP__

# include <sihd/util/IRunnable.hpp>
# include <mutex>
# include <list>
# include <cstdlib>

namespace sihd::util::atexit
{

// adds handler to be run post-exit
void    add_handler(IRunnable *ptr);
// remove handler and does NOT delete it
void    remove_handler(IRunnable *ptr);
// remove all handlers and deletes them
void    clear_handlers();
// actual goodbye callback - calls handlers post-exit and deletes them
void    exit_callback();
// permits exit callback post-exit
bool    install();

}

#endif 