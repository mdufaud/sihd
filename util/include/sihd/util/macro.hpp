#ifndef __SIHD_UTIL_MACRO_HPP__
#define __SIHD_UTIL_MACRO_HPP__

#define __SIHD_STR__(s) #s
#define __SIHD_STRINGIFY__(s) __SIHD_STR__(s)

#define __SIHD_LOC__ __FILE__ ":" __SIHD_STRINGIFY__(__LINE__)
#define __SIHD_TODO__ __SIHD_LOC__ " [TODO] "

#define SIHD_DIE_NE(res, expected)                                                                                     \
 if ((res) != (expected))                                                                                              \
 {                                                                                                                     \
  throw std::runtime_error(__SIHD_LOC__                                                                                \
                           " death on '" __SIHD_STRINGIFY__(res) "' != '" __SIHD_STRINGIFY__(expected) "'");           \
 }

#define SIHD_DIE_FALSE(res) SIHD_DIE_NE(res, true)

// https://www.boost.org/doc/libs/1_62_0/boost/current_function.hpp

#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600))         \
    || defined(__ghs__)

# define __SIHD_FUNCTION__ __PRETTY_FUNCTION__

#elif defined(__DMC__) && (__DMC__ >= 0x810)

# define __SIHD_FUNCTION__ __PRETTY_FUNCTION__

#elif defined(__FUNCSIG__)

# define __SIHD_FUNCTION__ __FUNCSIG__

#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))

# define __SIHD_FUNCTION__ __FUNCTION__

#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)

# define __SIHD_FUNCTION__ __FUNC__

#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)

# define __SIHD_FUNCTION__ __func__

#elif defined(__cplusplus) && (__cplusplus >= 201103)

# define __SIHD_FUNCTION__ __func__

#else

# define __SIHD_FUNCTION__ "(unknown)"

#endif

#endif
