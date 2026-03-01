#ifndef __SIHD_UTIL_PLATFORM_HPP__
#define __SIHD_UTIL_PLATFORM_HPP__

#include <sys/types.h> // pid_t, ssize_t

// === Architecture ===

#if defined(__LP64__) || defined(_WIN64)
# define __SIHD_64BITS__
#else
# define __SIHD_32BITS__
#endif

#if defined(__EMSCRIPTEN__)
# define __SIHD_EMSCRIPTEN__
#endif

// === Platform macros ===

#if (defined(_WIN16) || defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(__MINGW32__))

# define __SIHD_WINDOWS__
# define __SIHD_PLATFORM__ "windows"

#elif (defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)                    \
       || defined(__CYGWIN__) || defined(__EMSCRIPTEN__))

# define __SIHD_UNIX__
# define __SIHD_LINUX__

# if defined(__ANDROID__)
#  define __SIHD_PLATFORM__ "android"
#  define __SIHD_ANDROID__
# else
#  define __SIHD_PLATFORM__ "linux"
# endif

#elif defined(_AIX) || defined(__TOS__AIX__)

# define __SIHD_UNIX__
# define __SIHD_AIX__
# define __SIHD_PLATFORM__ "aix"

#elif defined(__sun) || defined(sun) || defined(__SVR4) || defined(__svr4__)

# define __SIHD_UNIX__
# define __SIHD_SUN__
# define __SIHD_PLATFORM__ "solaris"

#elif defined(__APPLE__) || defined(__MACH__) || defined(macintosh)

# define __SIHD_UNIX__
# define __SIHD_APPLE__

# include <TargetConditionals.h>
# if TARGET_IPHONE_SIMULATOR == 1
#  define __SIHD_IOS__
#  define __SIHD_PLATFORM__ "ios"
# elif TARGET_OS_IPHONE == 1
#  define __SIHD_IOS__
#  define __SIHD_PLATFORM__ "ios"
# elif TARGET_OS_MAC == 1
#  define __SIHD_PLATFORM__ "osx"
# endif

#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)

# if defined(__NetBSD__)
#  define __SIHD_NET_BSD__
#  define __SIHD_PLATFORM__ "net-bsd"
# elif defined(__FreeBSD__)
#  define __SIHD_FREE_BSD__
#  define __SIHD_PLATFORM__ "free-bsd"
# elif defined(__DragonFly__)
#  define __SIHD_DRAGON_FLY__
#  define __SIHD_PLATFORM__ "dragon-fly"
# elif defined(__OpenBSD__)
#  define __SIHD_OPEN_BSD__
#  define __SIHD_PLATFORM__ "open-bsd"
# else
#  define __SIHD_PLATFORM__ "bsd"
# endif

# define __SIHD_UNIX__
# define __SIHD_BSD__

#endif

// === Platform typedefs ===

#if !defined(__SIHD_WINDOWS__)
# include <sys/resource.h> // rlim_t
# include <sys/socket.h>   // socklen_t
#else
typedef int socklen_t;
typedef unsigned long rlim_t;
// missing mingw — int and not uint (libwebsockets)
typedef int uid_t;
#endif

// === Platform constexpr booleans ===

namespace sihd::util::platform
{

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

#if defined(__SIHD_EMSCRIPTEN__)
constexpr bool is_emscripten = true;
#else
constexpr bool is_emscripten = false;
#endif

#if defined(__SANITIZE_ADDRESS__)
constexpr bool is_run_with_asan = true;
#else
constexpr bool is_run_with_asan = false;
#endif

} // namespace sihd::util::platform

#endif