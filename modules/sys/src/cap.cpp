#include <algorithm>
#include <array>

#include <sihd/sys/cap.hpp>

namespace sihd::sys::cap
{

namespace
{

struct CapName
{
        Cap cap;
        std::string_view name;
};

// linux capability names, kept as the portable identifier on every platform
constexpr std::array<CapName, count> cap_names = {{
    {Cap::chown, "cap_chown"},
    {Cap::dac_override, "cap_dac_override"},
    {Cap::fowner, "cap_fowner"},
    {Cap::kill, "cap_kill"},
    {Cap::setgid, "cap_setgid"},
    {Cap::setuid, "cap_setuid"},
    {Cap::net_admin, "cap_net_admin"},
    {Cap::net_raw, "cap_net_raw"},
    {Cap::net_bind_service, "cap_net_bind_service"},
    {Cap::sys_admin, "cap_sys_admin"},
    {Cap::sys_ptrace, "cap_sys_ptrace"},
    {Cap::sys_time, "cap_sys_time"},
#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
    {Cap::dac_read_search, "cap_dac_read_search"},
    {Cap::fsetid, "cap_fsetid"},
    {Cap::setpcap, "cap_setpcap"},
    {Cap::linux_immutable, "cap_linux_immutable"},
    {Cap::net_broadcast, "cap_net_broadcast"},
    {Cap::ipc_lock, "cap_ipc_lock"},
    {Cap::ipc_owner, "cap_ipc_owner"},
    {Cap::sys_module, "cap_sys_module"},
    {Cap::sys_rawio, "cap_sys_rawio"},
    {Cap::sys_chroot, "cap_sys_chroot"},
    {Cap::sys_pacct, "cap_sys_pacct"},
    {Cap::sys_boot, "cap_sys_boot"},
    {Cap::sys_nice, "cap_sys_nice"},
    {Cap::sys_resource, "cap_sys_resource"},
    {Cap::sys_tty_config, "cap_sys_tty_config"},
    {Cap::mknod, "cap_mknod"},
    {Cap::lease, "cap_lease"},
    {Cap::audit_write, "cap_audit_write"},
    {Cap::audit_control, "cap_audit_control"},
    {Cap::setfcap, "cap_setfcap"},
    {Cap::mac_override, "cap_mac_override"},
    {Cap::mac_admin, "cap_mac_admin"},
    {Cap::syslog, "cap_syslog"},
    {Cap::wake_alarm, "cap_wake_alarm"},
    {Cap::block_suspend, "cap_block_suspend"},
    {Cap::audit_read, "cap_audit_read"},
    {Cap::perfmon, "cap_perfmon"},
    {Cap::bpf, "cap_bpf"},
    {Cap::checkpoint_restore, "cap_checkpoint_restore"},
#endif
}};

constexpr std::array<Cap, count> cap_list = [] {
    std::array<Cap, count> caps {};
    for (size_t i = 0; i < cap_names.size(); ++i)
        caps[i] = cap_names[i].cap;
    return caps;
}();

// every enumerator, in declaration order, each with a name - a forgotten entry leaves a
// value-initialised {chown, ""} slot
static_assert([] {
    for (size_t i = 0; i < count; ++i)
    {
        if (static_cast<size_t>(cap_names[i].cap) != i || cap_names[i].name.empty())
            return false;
    }
    return true;
}());

} // namespace

std::span<const Cap> all()
{
    return cap_list;
}

std::string_view to_name(Cap cap)
{
    const auto it = std::ranges::find(cap_names, cap, &CapName::cap);
    return it != cap_names.end() ? it->name : std::string_view {};
}

std::optional<Cap> from_name(std::string_view name)
{
    const auto it = std::ranges::find(cap_names, name, &CapName::name);
    if (it == cap_names.end())
        return std::nullopt;
    return it->cap;
}

} // namespace sihd::sys::cap
