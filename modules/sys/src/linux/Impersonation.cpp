#include <sihd/sys/Impersonation.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

// the per-thread identity switch is a linux kernel behaviour: emscripten has no syscall() and
// apple/bsd have no setresuid at all - Impersonation::supports_privileged is false there and the
// identity switch is a no-op
#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)

# define SIHD_IMPERSONATION_SETRES

# include <sys/syscall.h>
# include <unistd.h>

// The libc wrappers for setresuid()/setresgid() are process wide: POSIX requires the change to
// apply to the whole process, so both glibc (NPTL "setxid" broadcast) and musl (__synccall)
// replay the syscall on every thread. Verified on both libcs. The raw syscall keeps the kernel
// behaviour, which is per-thread - that is what thread scoped impersonation needs.
//
// Calling the syscall directly carries no licensing constraint: the kernel COPYING file
// excludes "user programs that use kernel services by normal system calls" from its GPL, and
// the kernel headers carry the matching Linux-syscall-note exception.
//
// 32 bit architectures keep the old 16 bit uid syscall under the unsuffixed name.
# if defined(SYS_setresuid32)
#  define SIHD_SYS_SETRESUID SYS_setresuid32
#  define SIHD_SYS_SETRESGID SYS_setresgid32
# else
#  define SIHD_SYS_SETRESUID SYS_setresuid
#  define SIHD_SYS_SETRESGID SYS_setresgid
# endif

#endif

namespace sihd::sys
{

SIHD_LOGGER;

#if defined(SIHD_IMPERSONATION_SETRES)

namespace
{

constexpr uid_t keep_uid = static_cast<uid_t>(-1);
constexpr gid_t keep_gid = static_cast<gid_t>(-1);

// only the effective id is moved: leaving the real and saved ids untouched is what keeps the
// switch reversible
bool set_thread_euid(uid_t uid)
{
    return syscall(SIHD_SYS_SETRESUID, keep_uid, uid, keep_uid) == 0;
}

bool set_thread_egid(gid_t gid)
{
    return syscall(SIHD_SYS_SETRESGID, keep_gid, gid, keep_gid) == 0;
}

} // namespace

struct Impersonation::Impl
{
        uid_t previous_uid = 0;
        gid_t previous_gid = 0;
        bool active = false;
};

Impersonation::Impersonation(): _impl(std::make_unique<Impl>()) {}

Impersonation::~Impersonation()
{
    if (_impl->active && !this->revert())
        SIHD_LOG(critical, "Impersonation: thread is left impersonating uid {}", geteuid());
}

bool Impersonation::impersonate_as(const user::UserId & user_id, const user::GroupId & group_id)
{
    if (_impl->active)
    {
        SIHD_LOG(error, "Impersonation: already impersonating");
        return false;
    }
    if (!user_id.valid() || !group_id.valid())
        return false;

    const uid_t previous_uid = geteuid();
    const gid_t previous_gid = getegid();

    // group first: dropping the effective uid may remove the right to change the group
    if (!set_thread_egid(group_id.native()))
    {
        SIHD_LOG(error, "Impersonation: setresgid({}) failed: {}", group_id.native(), os::last_error_str());
        return false;
    }
    if (!set_thread_euid(user_id.native()))
    {
        SIHD_LOG(error, "Impersonation: setresuid({}) failed: {}", user_id.native(), os::last_error_str());
        // put the group back, the switch is aborted
        set_thread_egid(previous_gid);
        return false;
    }

    _impl->previous_uid = previous_uid;
    _impl->previous_gid = previous_gid;
    _impl->active = true;
    return true;
}

bool Impersonation::revert()
{
    if (!_impl->active)
        return false;

    // uid first: the saved uid is what grants the right to restore the group
    if (!set_thread_euid(_impl->previous_uid))
    {
        SIHD_LOG(error, "Impersonation: cannot restore uid {}: {}", _impl->previous_uid, os::last_error_str());
        return false;
    }
    if (!set_thread_egid(_impl->previous_gid))
    {
        SIHD_LOG(error, "Impersonation: cannot restore gid {}: {}", _impl->previous_gid, os::last_error_str());
        return false;
    }
    _impl->active = false;
    return true;
}

bool Impersonation::impersonating() const
{
    return _impl->active;
}

#else

struct Impersonation::Impl
{
};

Impersonation::Impersonation(): _impl(std::make_unique<Impl>()) {}

Impersonation::~Impersonation() = default;

bool Impersonation::impersonate_as([[maybe_unused]] const user::UserId & user_id,
                                   [[maybe_unused]] const user::GroupId & group_id)
{
    return false;
}

bool Impersonation::revert()
{
    return false;
}

bool Impersonation::impersonating() const
{
    return false;
}

#endif

bool Impersonation::impersonate_with_credentials([[maybe_unused]] std::string_view user_name,
                                                 [[maybe_unused]] std::string_view password,
                                                 [[maybe_unused]] std::string_view domain)
{
    // unix authenticates through PAM, not through the identity switch: see impersonate_as
    return false;
}

} // namespace sihd::sys
