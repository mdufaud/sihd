#ifndef __SIHD_UTIL_OS_HPP__
# define __SIHD_UTIL_OS_HPP__

# include <string>
# include <dlfcn.h>
# include <sihd/util/Task.hpp>

namespace sihd::util::os
{

void    *load_lib(std::string lib_name);

// emergency calls for when memory fails
ssize_t  write(int fd, const char *s);
// emergency calls for when memory fails
ssize_t  write_endl(int fd, const char *s);
// emergency calls for when memory fails
void    write_number(int fd, int number);

// prints formatted backtrace into file descriptor
ssize_t backtrace(int fd);

std::string get_signal_name(int sig);

// called when signal is caught
void    signal_callback(int sig);
// adds a handler to be run when signal is catched
bool    add_signal_handler(int sig, IRunnable *runnable);
// remove and deletes all handlers attached to this signal
bool    clear_signal_handlers(int sig);
// remove and deletes all handlers
bool    clear_signal_handlers();
// remove a single handler - if you have the ptr to remove the handler, means you can delete it
bool    clear_signal_handler(int sig, IRunnable *runnable);

// set signal handling to default - already taken care of, do not call
bool    _unhandle_signal(int sig);

}

#endif 