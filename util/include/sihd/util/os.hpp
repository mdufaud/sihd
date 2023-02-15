#ifndef __SIHD_UTIL_OS_HPP__
# define __SIHD_UTIL_OS_HPP__

# include <string>
# include <string_view>

# include <sihd/util/platform.hpp>

# if !defined(__SIHD_WINDOWS__)

#  include <sys/socket.h> // socklen_t
#  include <sys/resource.h> // rlim_t

typedef socklen_t sihd_socklen_t;
typedef rlim_t sihd_rlim_t;
typedef uid_t sihd_uid_t;

# else

typedef int sihd_socklen_t;
typedef unsigned long sihd_rlim_t;
typedef unsigned int sihd_uid_t;

# endif

# include <sihd/util/IHandler.hpp>

namespace sihd::util::os
{

std::string signal_name(int sig);

// adds a handler to be run when signal is catched
bool add_signal_handler(int sig, IHandler<int> *handler);
// remove and deletes all handlers attached to this signal
bool clear_signal_handlers(int sig);
// remove and deletes all handlers
bool clear_signal_handlers();
// remove a single handler - if you have the ptr to remove the handler, means you can delete it
bool clear_signal_handler(int sig, IHandler<int> *handler);

// set signal handling to default - already taken care of, do not call
bool unhandle_signal(int sig);

// syscalls linux - windows
bool ioctl(int fd, unsigned long request, void *arg_ptr = nullptr, bool logerror = false);
bool stat(const char *pathname, struct stat *statbuf, bool logerror = false);
bool fstat(int fd, struct stat *statbuf, bool logerror = false);

bool setsockopt(int socket, int level, int optname, const void *optval, sihd_socklen_t optlen, bool logerror = false);
bool getsockopt(int socket, int level, int optname, void *optval, sihd_socklen_t *optlen, bool logerror = false);

sihd_rlim_t max_fds();

bool is_root();

// debuggers
bool is_run_by_debugger();
bool is_run_by_valgrind();
bool is_run_with_asan();

// lib
void *load_symbol_unload_lib(const std::string & lib_name, std::string_view sym_name);

pid_t pid();
bool kill(pid_t pid, int sig);

void *load_lib(const std::string & lib_name);
void *load_symbol(void *handle, std::string_view sym_name);
bool close_lib(void *handle);
std::string lib_error();

// prints formatted backtrace into file descriptor
ssize_t backtrace(int fd, size_t backtrace_size = -1);

// bytes
ssize_t peak_rss();
// bytes
ssize_t current_rss();

#if defined(__SIHD_WINDOWS__)
constexpr bool is_windows = true;
constexpr bool is_unix = false;
#else
constexpr bool is_windows = false;
constexpr bool is_unix = true;
#endif

#if defined(__SIHD_ANDROID__)
constexpr bool is_android = true;
#else
constexpr bool is_android = false;
#endif

#if defined(__SIHD_APPLE__)
constexpr bool is_apple = true;
#else
constexpr bool is_apple = false;
#endif

#if defined(__SIHD_IOS__)
constexpr bool is_ios = true;
#else
constexpr bool is_ios = false;
#endif

#if defined(__EMSCRIPTEN__)
constexpr bool is_emscripten = true;
#else
constexpr bool is_emscripten = false;
#endif

}

#endif
