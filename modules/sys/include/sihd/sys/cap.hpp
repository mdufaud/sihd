#ifndef __SIHD_SYS_CAP_HPP__
#define __SIHD_SYS_CAP_HPP__

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <sihd/util/build.hpp>

namespace sihd::sys
{

// linux naming on every platform: libcap platforms carry the full kernel list, the others a portable subset
enum class Cap
{
    chown,
    dac_override,
    fowner,
    kill,
    setgid,
    setuid,
    net_admin,
    net_raw,
    net_bind_service,
    sys_admin,
    sys_ptrace,
    sys_time,
#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
    // kernel order
    dac_read_search,
    fsetid,
    setpcap,
    linux_immutable,
    net_broadcast,
    ipc_lock,
    ipc_owner,
    sys_module,
    sys_rawio,
    sys_chroot,
    sys_pacct,
    sys_boot,
    sys_nice,
    sys_resource,
    sys_tty_config,
    mknod,
    lease,
    audit_write,
    audit_control,
    setfcap,
    mac_override,
    mac_admin,
    syslog,
    wake_alarm,
    block_suspend,
    audit_read,
    perfmon,
    bpf,
    checkpoint_restore,
#endif
};

namespace cap
{

constexpr bool supported = sihd::util::build::is_windows
                           || (sihd::util::build::is_linux && !sihd::util::build::is_android
                               && !sihd::util::build::is_emscripten);

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
constexpr size_t count = 41;
#else
constexpr size_t count = 12;
#endif
std::span<const Cap> all();

std::string_view to_name(Cap cap);
std::optional<Cap> from_name(std::string_view name);

// true if the capability has a counterpart on this platform (windows ones only approximate the linux effect)
bool available(Cap cap);

bool has(Cap cap);

// linux only, false elsewhere; ambient is per-thread and kept across execve, bounding is process-wide, irreversible
bool set_ambient(Cap cap, bool active);
bool ambient(Cap cap);
bool drop_bounding(Cap cap);
bool bounding(Cap cap);

} // namespace cap

} // namespace sihd::sys

#endif
