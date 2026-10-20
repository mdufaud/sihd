#ifndef __SIHD_SYS_LINUX_PRCTL_HPP__
#define __SIHD_SYS_LINUX_PRCTL_HPP__

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_::prctl
{

constexpr bool supported = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

// fires when the parent thread exits, not the parent process; 0 clears it
sihd::util::Result<void> set_pdeathsig(int sig);
sihd::util::Result<int> pdeathsig();

sihd::util::Result<void> set_dumpable(bool active);
sihd::util::Result<bool> dumpable();

// irreversible, inherited across fork and execve
sihd::util::Result<void> set_no_new_privs();
sihd::util::Result<bool> no_new_privs();

sihd::util::Result<void> set_child_subreaper(bool active);
sihd::util::Result<bool> child_subreaper();

} // namespace sihd::sys::linux_::prctl

#endif
