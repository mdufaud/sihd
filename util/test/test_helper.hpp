#ifndef __SIHD_UTIL_TEST_HELPER_HPP__
#define __SIHD_UTIL_TEST_HELPER_HPP__

#include <cstdlib>
#include <cstring>

namespace test
{

inline bool is_run_by_valgrind()
{
    const char *ldpreload = std::getenv("LD_PRELOAD");
    return ldpreload != nullptr
           && (std::strstr(ldpreload, "/valgrind/") != nullptr || std::strstr(ldpreload, "/vgpreload") != nullptr);
}

} // namespace test

#endif
