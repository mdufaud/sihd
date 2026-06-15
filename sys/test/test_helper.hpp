#ifndef __SIHD_SYS_TEST_HELPER_HPP__
#define __SIHD_SYS_TEST_HELPER_HPP__

#include <string>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>

namespace test
{

// path to the sihd_sys_test_helper binary built next to the module binaries
inline std::string helper_path()
{
    namespace fs = sihd::sys::fs;
    // executable_path() -> <build>/test/bin/sihd_sys[.exe], helper -> <build>/bin/sihd_sys_test_helper
    const std::string build_dir = fs::parent(fs::parent(fs::parent(fs::executable_path())));
#if defined(__SIHD_WINDOWS__)
    return fs::combine(build_dir, "bin/sihd_sys_test_helper.exe");
#else
    return fs::combine(build_dir, "bin/sihd_sys_test_helper");
#endif
}

} // namespace test

#endif
