#include <sihd/sys/CapabilitySet.hpp>
#include <sihd/sys/cap.hpp>
#include <sihd/util/Logger.hpp>

// libcap is linux only: cap::supported is false elsewhere (android/emscripten/apple/bsd) and
// every operation is a no-op.
#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# define SIHD_CAPABILITIES_LIBCAP
# include <sys/capability.h>
// cross toolchains ship pre-5.8 kernel headers without the newest caps - the numbers are
// frozen kernel ABI, define the missing ones
# if !defined(CAP_PERFMON)
#  define CAP_PERFMON 38
# endif
# if !defined(CAP_BPF)
#  define CAP_BPF 39
# endif
# if !defined(CAP_CHECKPOINT_RESTORE)
#  define CAP_CHECKPOINT_RESTORE 40
# endif
#endif

namespace sihd::sys
{

SIHD_LOGGER;

#if defined(SIHD_CAPABILITIES_LIBCAP)

namespace
{

// no default case on purpose: -Wswitch rejects a new enumerator at compile time
std::optional<cap_value_t> to_cap_value(Cap cap)
{
    switch (cap)
    {
        case Cap::chown:
            return CAP_CHOWN;
        case Cap::dac_override:
            return CAP_DAC_OVERRIDE;
        case Cap::fowner:
            return CAP_FOWNER;
        case Cap::kill:
            return CAP_KILL;
        case Cap::setgid:
            return CAP_SETGID;
        case Cap::setuid:
            return CAP_SETUID;
        case Cap::net_admin:
            return CAP_NET_ADMIN;
        case Cap::net_raw:
            return CAP_NET_RAW;
        case Cap::net_bind_service:
            return CAP_NET_BIND_SERVICE;
        case Cap::sys_admin:
            return CAP_SYS_ADMIN;
        case Cap::sys_ptrace:
            return CAP_SYS_PTRACE;
        case Cap::sys_time:
            return CAP_SYS_TIME;
        case Cap::dac_read_search:
            return CAP_DAC_READ_SEARCH;
        case Cap::fsetid:
            return CAP_FSETID;
        case Cap::setpcap:
            return CAP_SETPCAP;
        case Cap::linux_immutable:
            return CAP_LINUX_IMMUTABLE;
        case Cap::net_broadcast:
            return CAP_NET_BROADCAST;
        case Cap::ipc_lock:
            return CAP_IPC_LOCK;
        case Cap::ipc_owner:
            return CAP_IPC_OWNER;
        case Cap::sys_module:
            return CAP_SYS_MODULE;
        case Cap::sys_rawio:
            return CAP_SYS_RAWIO;
        case Cap::sys_chroot:
            return CAP_SYS_CHROOT;
        case Cap::sys_pacct:
            return CAP_SYS_PACCT;
        case Cap::sys_boot:
            return CAP_SYS_BOOT;
        case Cap::sys_nice:
            return CAP_SYS_NICE;
        case Cap::sys_resource:
            return CAP_SYS_RESOURCE;
        case Cap::sys_tty_config:
            return CAP_SYS_TTY_CONFIG;
        case Cap::mknod:
            return CAP_MKNOD;
        case Cap::lease:
            return CAP_LEASE;
        case Cap::audit_write:
            return CAP_AUDIT_WRITE;
        case Cap::audit_control:
            return CAP_AUDIT_CONTROL;
        case Cap::setfcap:
            return CAP_SETFCAP;
        case Cap::mac_override:
            return CAP_MAC_OVERRIDE;
        case Cap::mac_admin:
            return CAP_MAC_ADMIN;
        case Cap::syslog:
            return CAP_SYSLOG;
        case Cap::wake_alarm:
            return CAP_WAKE_ALARM;
        case Cap::block_suspend:
            return CAP_BLOCK_SUSPEND;
        case Cap::audit_read:
            return CAP_AUDIT_READ;
        case Cap::perfmon:
            return CAP_PERFMON;
        case Cap::bpf:
            return CAP_BPF;
        case Cap::checkpoint_restore:
            return CAP_CHECKPOINT_RESTORE;
    }
    return std::nullopt;
}

// reads one flag out of a freshly loaded snapshot of the calling thread
bool live_flag_set(Cap cap, cap_flag_t flag)
{
    const auto value = to_cap_value(cap);
    if (!value.has_value())
        return false;
    cap_t caps = cap_get_proc();
    if (caps == nullptr)
        return false;
    cap_flag_value_t flag_value = CAP_CLEAR;
    const bool ok = cap_get_flag(caps, *value, flag, &flag_value) == 0;
    cap_free(caps);
    return ok && flag_value == CAP_SET;
}

} // namespace

namespace cap
{

bool available(Cap cap)
{
    return to_cap_value(cap).has_value();
}

bool has(Cap cap)
{
    return live_flag_set(cap, CAP_EFFECTIVE);
}

bool set_ambient(Cap cap, bool active)
{
    const auto value = to_cap_value(cap);
    return value.has_value() && cap_set_ambient(*value, active ? CAP_SET : CAP_CLEAR) == 0;
}

bool ambient(Cap cap)
{
    const auto value = to_cap_value(cap);
    return value.has_value() && cap_get_ambient(*value) == 1;
}

bool drop_bounding(Cap cap)
{
    const auto value = to_cap_value(cap);
    return value.has_value() && cap_drop_bound(*value) == 0;
}

bool bounding(Cap cap)
{
    const auto value = to_cap_value(cap);
    return value.has_value() && cap_get_bound(*value) == 1;
}

} // namespace cap

struct CapabilitySet::Impl
{
        cap_t caps = nullptr;

        ~Impl() { this->clear(); }

        void clear()
        {
            if (caps != nullptr)
            {
                cap_free(caps);
                caps = nullptr;
            }
        }

        bool flag_set(Cap cap, cap_flag_t flag) const
        {
            const auto cap_value = to_cap_value(cap);
            if (caps == nullptr || !cap_value.has_value())
                return false;
            cap_flag_value_t value = CAP_CLEAR;
            if (cap_get_flag(caps, *cap_value, flag, &value) != 0)
                return false;
            return value == CAP_SET;
        }

        bool set_flag(Cap cap, cap_flag_t flag, bool active)
        {
            auto cap_value = to_cap_value(cap);
            if (caps == nullptr || !cap_value.has_value())
                return false;
            return cap_set_flag(caps, flag, 1, &cap_value.value(), active ? CAP_SET : CAP_CLEAR) == 0;
        }
};

CapabilitySet::CapabilitySet(): _impl(std::make_unique<Impl>())
{
    this->refresh();
}

CapabilitySet::~CapabilitySet() = default;

bool CapabilitySet::refresh()
{
    _impl->clear();
    _impl->caps = cap_get_proc();
    if (_impl->caps == nullptr)
    {
        SIHD_LOG(error, "CapabilitySet: cap_get_proc failed");
        return false;
    }
    return true;
}

bool CapabilitySet::is_enabled(Cap cap) const
{
    return _impl->flag_set(cap, CAP_EFFECTIVE);
}

bool CapabilitySet::permitted(Cap cap) const
{
    return _impl->flag_set(cap, CAP_PERMITTED);
}

std::vector<Cap> CapabilitySet::enabled() const
{
    std::vector<Cap> ret;
    for (Cap cap : cap::all())
    {
        if (this->is_enabled(cap))
            ret.push_back(cap);
    }
    return ret;
}

bool CapabilitySet::raise(Cap cap)
{
    if (!this->permitted(cap))
        return false;
    return _impl->set_flag(cap, CAP_EFFECTIVE, true);
}

bool CapabilitySet::drop(Cap cap)
{
    return _impl->set_flag(cap, CAP_EFFECTIVE, false);
}

void CapabilitySet::drop_all()
{
    for (Cap cap : cap::all())
        _impl->set_flag(cap, CAP_EFFECTIVE, false);
}

bool CapabilitySet::apply()
{
    if (_impl->caps == nullptr)
        return false;
    if (cap_set_proc(_impl->caps) != 0)
    {
        SIHD_LOG(error, "CapabilitySet: cap_set_proc failed");
        return false;
    }
    return true;
}

#else

struct CapabilitySet::Impl
{
};

namespace cap
{

bool available([[maybe_unused]] Cap cap)
{
    return false;
}

bool has([[maybe_unused]] Cap cap)
{
    return false;
}

bool set_ambient([[maybe_unused]] Cap cap, [[maybe_unused]] bool active)
{
    return false;
}

bool ambient([[maybe_unused]] Cap cap)
{
    return false;
}

bool drop_bounding([[maybe_unused]] Cap cap)
{
    return false;
}

bool bounding([[maybe_unused]] Cap cap)
{
    return false;
}

} // namespace cap

CapabilitySet::CapabilitySet(): _impl(std::make_unique<Impl>()) {}

CapabilitySet::~CapabilitySet() = default;

bool CapabilitySet::refresh()
{
    return false;
}

bool CapabilitySet::is_enabled([[maybe_unused]] Cap cap) const
{
    return false;
}

bool CapabilitySet::permitted([[maybe_unused]] Cap cap) const
{
    return false;
}

std::vector<Cap> CapabilitySet::enabled() const
{
    return {};
}

bool CapabilitySet::raise([[maybe_unused]] Cap cap)
{
    return false;
}

bool CapabilitySet::drop([[maybe_unused]] Cap cap)
{
    return false;
}

void CapabilitySet::drop_all() {}

bool CapabilitySet::apply()
{
    return false;
}

#endif

} // namespace sihd::sys
