#ifndef __SIHD_UTIL_OS_HPP__
# define __SIHD_UTIL_OS_HPP__

# include <sihd/util/platform.hpp>
# include <sihd/util/IRunnable.hpp>

# include <sys/stat.h> // stat

# if !defined(__SIHD_WINDOWS__)
#  include <dlfcn.h>
#  include <sys/ioctl.h>
#  include <sys/socket.h>
#  include <sys/resource.h>
# else
#  include <winsock2.h>
#  include <ws2def.h>
#  include <winsock.h>
#  include <ws2tcpip.h>
typedef unsigned long rlim_t;
# endif

# include <string>
# include <map>
# include <list>
# include <mutex>

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

        // syscalls linux - windows
        static bool ioctl(int fd, unsigned long request, void *arg_ptr = nullptr, bool logerror = false);
        static bool stat(const char *pathname, struct stat *statbuf, bool logerror = false);
        static bool fstat(int fd, struct stat *statbuf, bool logerror = false);

        static bool setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror = false);
        static bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror = false);

        static rlim_t get_max_fds();

        static std::string get_executable_path();

        // debuggers
        static bool is_run_by_debugger();
        static bool is_run_by_valgrind();

        // lib
        static void *load_symbol_unload_lib(const std::string & lib_name, const std::string & sym_name);
# if !defined(__SIHD_WINDOWS__)
        static std::string get_error_lib();
        static void *load_lib(const std::string & lib_name);
        static void *get_symbol_lib(void *handle, const std::string & sym_name);
        static bool close_lib(void *handle);
# endif

# if !defined(__SIHD_WINDOWS__)
        static int get_interface_idx(const std::string & name);
        static std::string get_interface_mac_addr(const std::string & name);
        static std::string get_interface_ip_addr(const std::string & name);
# endif

        // backtrace
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

        static size_t  get_peak_rss();
        static size_t  get_current_rss();
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