#include <windows.h>

#include <map>
#include <vector>

#include <sihd/sys/CapabilitySet.hpp>
#include <sihd/sys/cap.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys
{

SIHD_LOGGER;

namespace
{

// windows privileges are not capabilities: only the ones with a comparable effect are mapped,
// the rest are reported unavailable
const char *privilege_name(Cap cap)
{
    switch (cap)
    {
        case Cap::chown:
            return SE_TAKE_OWNERSHIP_NAME;
        case Cap::dac_override:
            return SE_BACKUP_NAME;
        case Cap::fowner:
            return SE_RESTORE_NAME;
        case Cap::sys_ptrace:
            return SE_DEBUG_NAME;
        case Cap::sys_time:
            return SE_SYSTEMTIME_NAME;
        // no windows counterpart
        case Cap::kill:
        case Cap::setgid:
        case Cap::setuid:
        case Cap::net_admin:
        case Cap::net_raw:
        case Cap::net_bind_service:
        case Cap::sys_admin:
            return nullptr;
    }
    return nullptr;
}

bool privilege_luid(Cap cap, LUID *luid)
{
    const char *name = privilege_name(cap);
    if (name == nullptr)
        return false;
    return LookupPrivilegeValueA(nullptr, name, luid) != 0;
}

// live check against the process token, without going through a snapshot
bool live_privilege_enabled(Cap cap)
{
    LUID luid;
    if (!privilege_luid(cap, &luid))
        return false;

    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == 0)
        return false;

    PRIVILEGE_SET privileges;
    privileges.PrivilegeCount = 1;
    privileges.Control = PRIVILEGE_SET_ALL_NECESSARY;
    privileges.Privilege[0].Luid = luid;
    privileges.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = FALSE;
    const bool ok = PrivilegeCheck(token, &privileges, &result) != 0;
    CloseHandle(token);
    return ok && result != FALSE;
}

} // namespace

namespace cap
{

bool available(Cap cap)
{
    return privilege_name(cap) != nullptr;
}

bool has(Cap cap)
{
    return live_privilege_enabled(cap);
}

// ambient/bounding sets are linux specific, no windows equivalent

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

struct CapabilitySet::Impl
{
        // query handle only - apply() asks for the adjust rights when it needs them
        HANDLE token = nullptr;
        // privilege held by the token
        std::map<Cap, bool> present;
        // privilege currently enabled on the process
        std::map<Cap, bool> enabled;
        // wanted state, committed by apply()
        std::map<Cap, bool> desired;

        ~Impl() { this->close(); }

        void close()
        {
            if (token != nullptr)
            {
                CloseHandle(token);
                token = nullptr;
            }
        }
};

CapabilitySet::CapabilitySet(): _impl(std::make_unique<Impl>())
{
    this->refresh();
}

CapabilitySet::~CapabilitySet() = default;

bool CapabilitySet::refresh()
{
    _impl->close();
    _impl->present.clear();
    _impl->enabled.clear();
    _impl->desired.clear();

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &_impl->token) == 0)
    {
        SIHD_LOG(error, "CapabilitySet: OpenProcessToken failed");
        return false;
    }

    DWORD size = 0;
    GetTokenInformation(_impl->token, TokenPrivileges, nullptr, 0, &size);
    if (size == 0)
    {
        SIHD_LOG(error, "CapabilitySet: GetTokenInformation returned no size");
        return false;
    }

    std::vector<BYTE> buffer(size);
    if (GetTokenInformation(_impl->token, TokenPrivileges, buffer.data(), size, &size) == 0)
    {
        SIHD_LOG(error, "CapabilitySet: GetTokenInformation failed");
        return false;
    }

    const TOKEN_PRIVILEGES *token_privileges = reinterpret_cast<const TOKEN_PRIVILEGES *>(buffer.data());
    for (Cap cap : cap::all())
    {
        LUID luid;
        bool is_present = false;
        bool is_enabled = false;
        if (privilege_luid(cap, &luid))
        {
            for (DWORD i = 0; i < token_privileges->PrivilegeCount; ++i)
            {
                const LUID_AND_ATTRIBUTES & entry = token_privileges->Privileges[i];
                if (entry.Luid.LowPart == luid.LowPart && entry.Luid.HighPart == luid.HighPart)
                {
                    is_present = true;
                    is_enabled = (entry.Attributes & SE_PRIVILEGE_ENABLED) != 0;
                    break;
                }
            }
        }
        _impl->present[cap] = is_present;
        _impl->enabled[cap] = is_enabled;
        _impl->desired[cap] = is_enabled;
    }
    return true;
}

bool CapabilitySet::is_enabled(Cap cap) const
{
    const auto it = _impl->desired.find(cap);
    return it != _impl->desired.end() && it->second;
}

bool CapabilitySet::permitted(Cap cap) const
{
    const auto it = _impl->present.find(cap);
    return it != _impl->present.end() && it->second;
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
    _impl->desired[cap] = true;
    return true;
}

bool CapabilitySet::drop(Cap cap)
{
    if (_impl->token == nullptr)
        return false;
    // dropping what is not held is a no-op, like on linux
    _impl->desired[cap] = false;
    return true;
}

void CapabilitySet::drop_all()
{
    for (auto & [cap, wanted] : _impl->desired)
        wanted = false;
}

bool CapabilitySet::apply()
{
    if (_impl->token == nullptr)
        return false;

    // adjust rights asked for only now
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token) == 0)
    {
        SIHD_LOG(error, "CapabilitySet: OpenProcessToken failed");
        return false;
    }

    bool success = true;
    for (const auto & [cap, wanted] : _impl->desired)
    {
        if (_impl->enabled[cap] == wanted)
            continue;

        LUID luid;
        if (!privilege_luid(cap, &luid))
            continue;

        TOKEN_PRIVILEGES token_privileges;
        token_privileges.PrivilegeCount = 1;
        token_privileges.Privileges[0].Luid = luid;
        token_privileges.Privileges[0].Attributes = wanted ? SE_PRIVILEGE_ENABLED : 0;

        SetLastError(ERROR_SUCCESS);
        AdjustTokenPrivileges(token, FALSE, &token_privileges, 0, nullptr, nullptr);
        // AdjustTokenPrivileges succeeds even when the privilege was not assigned
        if (GetLastError() != ERROR_SUCCESS)
        {
            SIHD_LOG(error, "CapabilitySet: cannot set privilege {}", cap::to_name(cap));
            success = false;
            continue;
        }
        _impl->enabled[cap] = wanted;
    }
    CloseHandle(token);
    return success;
}

} // namespace sihd::sys
