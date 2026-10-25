#ifndef __SIHD_SYS_TEST_USERNS_CHILD_HPP__
#define __SIHD_SYS_TEST_USERNS_CHILD_HPP__

#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <fmt/format.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/linux/ns.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Error.hpp>

namespace test
{

// exit codes owned by expect_in_userns: a body returns 0 on success, below them otherwise
inline constexpr int userns_denied = 100;
inline constexpr int userns_multithreaded = 101;
inline constexpr int userns_unshare_failed = 102;
inline constexpr int userns_map_failed = 103;

inline void expect_in_userns(std::span<const sihd::sys::linux_::ns::Type> extra_types,
                             const std::function<int()> & body,
                             const std::map<int, std::string> & skipped_codes = {})
{
    auto apparmor_restrict = sihd::sys::fs::read_all("/proc/sys/kernel/apparmor_restrict_unprivileged_userns");
    if (apparmor_restrict && apparmor_restrict->starts_with('1'))
        GTEST_SKIP() << "user namespaces confined by apparmor";

    std::vector<sihd::sys::linux_::ns::Type> types = {sihd::sys::linux_::ns::Type::user};
    types.insert(types.end(), extra_types.begin(), extra_types.end());
    const uint32_t uid = sihd::sys::user::effective_user().native();
    const uint32_t gid = sihd::sys::user::effective_group().native();
    sihd::sys::Process proc([&types, uid, gid, &body]() -> int {
        auto unshared = sihd::sys::linux_::ns::unshare(types);
        if (!unshared && unshared.error().code == sihd::util::ErrorCode::permission_denied)
            return userns_denied;
        // unshare(CLONE_NEWUSER) refuses a multi-threaded caller, which a qemu-user child is
        if (!unshared && unshared.error().code == sihd::util::ErrorCode::invalid_argument)
            return userns_multithreaded;
        if (!unshared)
            return userns_unshare_failed;
        if (!sihd::sys::fs::write("/proc/self/setgroups", "deny")
            || !sihd::sys::fs::write("/proc/self/gid_map", fmt::format("0 {} 1", gid))
            || !sihd::sys::fs::write("/proc/self/uid_map", fmt::format("0 {} 1", uid)))
            return userns_map_failed;
        return body();
    });
    proc.set_force_fork(true);
    ASSERT_TRUE(proc.execute());
    ASSERT_TRUE(proc.wait_exit().value_or(false));

    const int code = static_cast<int>(proc.return_code());
    if (code == userns_denied)
        GTEST_SKIP() << "user namespaces denied";
    if (code == userns_multithreaded)
        GTEST_SKIP() << "process is not single-threaded (qemu-user)";
    if (auto skipped = skipped_codes.find(code); skipped != skipped_codes.end())
        GTEST_SKIP() << skipped->second;
    EXPECT_EQ(code, 0);
}

} // namespace test

#endif
