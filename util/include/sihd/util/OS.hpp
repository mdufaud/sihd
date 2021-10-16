#ifndef __SIHD_UTIL_OS_HPP__
# define __SIHD_UTIL_OS_HPP__

# include <sihd/util/platform.hpp>

# include <string>
# if !defined(__SIHD_WINDOWS__)
#  include <dlfcn.h>
#  include <sys/ioctl.h>
# else
#  include <winsock.h>
# endif
# include <map>
# include <list>
# include <mutex>
# include <sihd/util/Task.hpp>

namespace sihd::util
{

class OS
{
    public:
        static std::string get_signal_name(int sig);

        // called when signal is caught
        static void signal_callback(int sig);
        // adds a handler to be run when signal is catched
        static bool add_signal_handler(int sig, IRunnable *runnable);
        // remove and deletes all handlers attached to this signal
        static bool clear_signal_handlers(int sig);
        // remove and deletes all handlers
        static bool clear_signal_handlers();
        // remove a single handler - if you have the ptr to remove the handler, means you can delete it
        static bool clear_signal_handler(int sig, IRunnable *runnable);

        // set signal handling to default - already taken care of, do not call
        static bool unhandle_signal(int sig);

        static int ioctl(int fd, unsigned long request, unsigned long *arg_ptr);

        static bool is_run_by_debugger();
        static bool is_run_by_valgrind();

# if !defined(__SIHD_WINDOWS__)
        static std::string get_error_lib();
        static void *load_lib(const std::string & lib_name);
        static void *get_symbol_lib(void *handle, const std::string & sym_name);
        static bool close_lib(void *handle);
# endif
        static void *load_symbol_unload_lib(const std::string & lib_name, const std::string & sym_name);

# if !defined(__SIHD_WINDOWS__)
        static const int backtrace_size;

        // emergency calls for when memory fails
        static ssize_t write(int fd, const char *s);
        // emergency calls for when memory fails
        static ssize_t write_endl(int fd, const char *s);
        // emergency calls for when memory fails
        static ssize_t write_number(int fd, int number);

        // prints formatted backtrace into file descriptor
        static ssize_t backtrace(int fd);
# endif

    private:
        OS() {};
        ~OS() {};

# if !defined(__SIHD_WINDOWS__)
        static void *backtrace_buffer[];
# endif

        static bool signal_used;
        static std::mutex signal_mutex;
        static std::map<int, std::list<IRunnable *>> map_signals_handlers;
};

}

#endif 