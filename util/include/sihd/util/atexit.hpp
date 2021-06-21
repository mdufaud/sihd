#ifndef __SIHD_UTIL_ATEXIT_HPP__
# define __SIHD_UTIL_ATEXIT_HPP__

# include <sihd/util/IRunnable.hpp>
# include <mutex>
# include <list>
# include <cstdlib>

namespace sihd::util::atexit
{

void    add_handler(IRunnable *ptr);
void    remove_handler(IRunnable *ptr);
void    clear_handlers();
void    exit_callback();
bool    install();

std::string get_signal_name(int sig);
void    signal_callback(int sig);
bool    handle_signal(int sig);
bool    remove_signal(int sig);
}

#endif 