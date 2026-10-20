#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sihd/sys/linux/prctl.hpp>

# if !defined(__SIHD_EMSCRIPTEN__)
#  include <sys/prctl.h>
# endif

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::linux_::prctl
{

# if !defined(__SIHD_EMSCRIPTEN__)

Result<void> set_pdeathsig(int sig)
{
    if (::prctl(PR_SET_PDEATHSIG, sig, 0, 0, 0) == -1)
        return std::unexpected(Error::from_errno("could not set parent death signal {}", sig));
    return {};
}

Result<int> pdeathsig()
{
    int sig = 0;
    if (::prctl(PR_GET_PDEATHSIG, &sig, 0, 0, 0) == -1)
        return std::unexpected(Error::from_errno("could not get parent death signal"));
    return sig;
}

Result<void> set_dumpable(bool active)
{
    if (::prctl(PR_SET_DUMPABLE, active ? 1 : 0, 0, 0, 0) == -1)
        return std::unexpected(Error::from_errno("could not set dumpable"));
    return {};
}

Result<bool> dumpable()
{
    const int ret = ::prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
    if (ret == -1)
        return std::unexpected(Error::from_errno("could not get dumpable"));
    return ret != 0;
}

Result<void> set_no_new_privs()
{
    if (::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1)
        return std::unexpected(Error::from_errno("could not set no_new_privs"));
    return {};
}

Result<bool> no_new_privs()
{
    const int ret = ::prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
    if (ret == -1)
        return std::unexpected(Error::from_errno("could not get no_new_privs"));
    return ret != 0;
}

Result<void> set_child_subreaper(bool active)
{
    if (::prctl(PR_SET_CHILD_SUBREAPER, active ? 1 : 0, 0, 0, 0) == -1)
        return std::unexpected(Error::from_errno("could not set child subreaper"));
    return {};
}

Result<bool> child_subreaper()
{
    int subreaper = 0;
    if (::prctl(PR_GET_CHILD_SUBREAPER, &subreaper, 0, 0, 0) == -1)
        return std::unexpected(Error::from_errno("could not get child subreaper"));
    return subreaper != 0;
}

# else

Result<void> set_pdeathsig(int)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<int> pdeathsig()
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> set_dumpable(bool)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<bool> dumpable()
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> set_no_new_privs()
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<bool> no_new_privs()
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> set_child_subreaper(bool)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<bool> child_subreaper()
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

# endif

} // namespace sihd::sys::linux_::prctl

#endif
