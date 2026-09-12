#ifndef __SIHD_SYS_TEST_HELPER_HPP__
#define __SIHD_SYS_TEST_HELPER_HPP__

#include <optional>
#include <string>
#include <string_view>

#include <sihd/sys/env.hpp>
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

// sets the variable for the scope; a nullopt value unsets it; the previous state is restored
struct ScopedEnv
{
        ScopedEnv(std::string_view name, std::optional<std::string_view> value): _name(name)
        {
            _previous = sihd::sys::env::get(name);
            if (value.has_value())
                sihd::sys::env::set(name, *value);
            else
                sihd::sys::env::unset(name);
        }

        ~ScopedEnv()
        {
            if (_previous.has_value())
                sihd::sys::env::set(_name, *_previous);
            else
                sihd::sys::env::unset(_name);
        }

        ScopedEnv(const ScopedEnv &) = delete;
        ScopedEnv & operator=(const ScopedEnv &) = delete;

        std::string _name;
        std::optional<std::string> _previous;
};

} // namespace test

#endif
