#include <stdexcept>

#include <ctype.h>
#include <cstring>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>

// pid, resource limits, socket ioctls, boot time, backtrace, memory queries,
// error strings and debugger detection live in src/linux|windows/os.cpp

namespace sihd::sys::os
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::os");

bool exists_in_path(std::string_view binary_name)
{
    const char *path = getenv("PATH");
    if (path == nullptr)
        return false;

    Splitter splitter(":");
    for (const std::string & subpath : splitter.split(path))
    {
        if (fs::is_executable(fs::combine(subpath, binary_name)))
            return true;
    }

    return false;
}

bool is_run_by_valgrind()
{
    char *ldpreload = getenv("LD_PRELOAD");
    return ldpreload != nullptr
           && (strstr(ldpreload, "/valgrind/") != nullptr || strstr(ldpreload, "/vgpreload") != nullptr);
}

bool is_run_by_qemu()
{
    // qemu-user passes its own QEMU_LD_PREFIX into the guest environment (cross sysroot).
    // No reliable non-env signal: qemu-11 emulates the vDSO and fakes uname/auxv to the guest.
    return getenv("QEMU_LD_PREFIX") != nullptr;
}

} // namespace sihd::sys::os
