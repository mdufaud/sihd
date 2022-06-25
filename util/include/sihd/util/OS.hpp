#ifndef __SIHD_UTIL_OS_HPP__
# define __SIHD_UTIL_OS_HPP__

# include <sihd/util/platform.hpp>

# include <string>
# include <map>
# include <list>
# include <mutex>

# include <signal.h>
# include <sys/stat.h>

# if !defined(__SIHD_WINDOWS__)

#  include <dlfcn.h>
#  include <sys/ioctl.h>
#  include <sys/socket.h>
#  include <sys/resource.h>

typedef rlim_t sihd_rlim_t;
typedef uid_t sihd_uid_t;

# else

#  include <winsock2.h>
#  include <ws2def.h>
#  include <winsock.h>
#  include <ws2tcpip.h>

typedef unsigned long sihd_rlim_t;
typedef unsigned int sihd_uid_t;

# endif

# include <sihd/util/IRunnable.hpp>
# include <sihd/util/IHandler.hpp>

namespace sihd::util
{

class OS
{
    public:
        OS() = delete;

        static std::string signal_name(int sig);

        // adds a handler to be run when signal is catched
        static bool add_signal_handler(int sig, IHandler<int> *handler);
        // remove and deletes all handlers attached to this signal
        static bool clear_signal_handlers(int sig);
        // remove and deletes all handlers
        static bool clear_signal_handlers();
        // remove a single handler - if you have the ptr to remove the handler, means you can delete it
        static bool clear_signal_handler(int sig, IHandler<int> *handler);

        // set signal handling to default - already taken care of, do not call
        static bool unhandle_signal(int sig);

        // syscalls linux - windows
        static bool ioctl(int fd, unsigned long request, void *arg_ptr = nullptr, bool logerror = false);
        static bool stat(const char *pathname, struct stat *statbuf, bool logerror = false);
        static bool fstat(int fd, struct stat *statbuf, bool logerror = false);

        static bool setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror = false);
        static bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror = false);

        static sihd_rlim_t max_fds();

        static bool is_root();

        // debuggers
        static bool is_run_by_debugger();
        static bool is_run_by_valgrind();
        static bool is_run_with_asan();

        // lib
        static void *load_symbol_unload_lib(const std::string & lib_name, std::string_view sym_name);

        static pid_t pid();
        static bool kill(pid_t pid, int sig);

# if !defined(__SIHD_WINDOWS__)
        static std::string lib_error();
        static void *load_lib(const std::string & lib_name);
        static void *load_symbol(void *handle, std::string_view sym_name);
        static bool close_lib(void *handle);
# endif

        // backtrace
        static int backtrace_size;
        // prints formatted backtrace into file descriptor
        static ssize_t backtrace(int fd);

        // bytes
        static ssize_t peak_rss();
        // bytes
        static ssize_t current_rss();

    protected:
        // called when signal is caught
        static void _signal_callback(int sig);

    private:
# if !defined(__SIHD_WINDOWS__)
        static void *backtrace_buffer[];

        // emergency calls for when memory fails
        static ssize_t write(int fd, const char *s);
        // emergency calls for when memory fails
        static ssize_t write_endl(int fd, const char *s);
        // emergency calls for when memory fails
        static ssize_t write_number(int fd, int number);
# endif
        static bool signal_used;
        static std::mutex signal_mutex;
        static std::map<int, std::list<IHandler<int> *>> map_signals_handlers;
};

}

#endif