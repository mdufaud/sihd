#ifndef __SIHD_UTIL_OS_HPP__
#define __SIHD_UTIL_OS_HPP__

#include <sys/types.h> // pid_t

#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/platform.hpp>

#if !defined(__SIHD_WINDOWS__)

# include <sys/resource.h> // rlim_t
# include <sys/socket.h>   // socklen_t

#else

typedef int socklen_t;
typedef unsigned long rlim_t;
// missing mingw
// int and not uint (libwebsockets)
typedef int uid_t;

#endif

namespace sihd::util::os
{

// syscalls linux - windows
bool ioctl(int fd, unsigned long request, void *arg_ptr = nullptr, bool logerror = false);
bool stat(const char *pathname, struct stat *statbuf, bool logerror = false);
bool fstat(int fd, struct stat *statbuf, bool logerror = false);

bool setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror = false);
bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror = false);

bool is_root();
rlim_t max_fds();

// debuggers
bool is_run_by_debugger();
bool is_run_by_valgrind();

pid_t pid();

// prints formatted backtrace into file descriptor
ssize_t backtrace(int fd, size_t backtrace_size = -1);

// bytes
ssize_t peak_rss();
// bytes
ssize_t current_rss();

#if defined(SIHD_STATIC)
constexpr bool is_statically_linked = true;
#else
constexpr bool is_statically_linked = false;
#endif

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

#if defined(__SANITIZE_ADDRESS__)
constexpr bool is_run_with_asan = true;
#else
constexpr bool is_run_with_asan = false;
#endif

} // namespace sihd::util::os

#endif
