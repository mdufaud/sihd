#ifndef PLATFORM_H__
# define PLATFORM_H__

# if defined(__linux__) || defined(__CYGWIN__)

#  define __SIHD_UNIX__
#  define __SIHD_LINUX__

# elif (defined(_WIN16) || defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(__MINGW32__))

#  define __SIHD_WINDOWS__

# elif defined(__sun) || defined(sun)

#  define __SIHD_UNIX__
#  define __SIHD_SUN__

# elif defined(__APPLE__) || defined(__MACH__) || defined(macintosh)

#  define __SIHD_UNIX__
#  define __SIHD_APPLE__

# elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)

#  if defined(__NetBSD__)
#   define __SIHD_NET_BSD__
#  elif defined(__FreeBSD__)
#   define __SIHD_FREE_BSD__
#  elif defined(__DragonFly__)
#   define __SIHD_DRAGON_FLY__
#  elif defined(__OpenBSD__)
#   define __SIHD_OPEN_BSD__
#  endif

#  define __SIHD_UNIX__
#  define __SIHD_BSD__

# endif

#endif