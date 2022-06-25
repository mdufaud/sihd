#ifndef __SIHD_UTIL_MACRO_HPP__
# define __SIHD_UTIL_MACRO_HPP__

# define __SIHD_STR__(s) #s
# define __SIHD_STRINGIFY__(s) __SIHD_STR__(s)

# define __SIHD_LOC__ __FILE__ ":" __SIHD_STRINGIFY__(__LINE__)
# define __SIHD_TODO__ __SIHD_LOC__ " TODO "

#endif