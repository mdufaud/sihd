#ifndef __SIHD_UTIL_ATEXIT_HPP__
#define __SIHD_UTIL_ATEXIT_HPP__

#include <cstdlib>
#include <list>
#include <mutex>

#include <sihd/util/IRunnable.hpp>

namespace sihd::util::atexit
{

// actual exit callback post-exit - calls handlers and deletes them
// you should not use it manually
void exit_callback();

// adds handler to be run post-exit
void add_handler(IRunnable *ptr);
// remove handler and does NOT delete it
void remove_handler(IRunnable *ptr);
// remove all handlers and deletes them
void clear_handlers();
// permits exit callback post-exit
bool install();

} // namespace sihd::util::atexit

#endif