#ifndef __SIHD_SYS_LINUX_NS_HPP__
#define __SIHD_SYS_LINUX_NS_HPP__

#include <cstdint>
#include <span>
#include <string_view>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_::ns
{

constexpr bool supported = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

enum class Type
{
    cgroup,
    ipc,
    mount,
    net,
    pid,
    pid_for_children,
    time,
    time_for_children,
    user,
    uts,
};

std::string_view to_str(Type type);

// a new user namespace maps no id until uid_map and gid_map are written
sihd::util::Result<void> unshare(Type type);
sihd::util::Result<void> unshare(std::span<const Type> types);
sihd::util::Result<void> setns(sihd::util::ZStringView path, Type type);
sihd::util::Result<void> setns(int fd, Type type);

sihd::util::Result<uint64_t> id(Type type, pid_t pid);
sihd::util::Result<uint64_t> id(Type type);

} // namespace sihd::sys::linux_::ns

#endif
