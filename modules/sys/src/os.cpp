#include <ctype.h>

#include <cstring>
#include <stdexcept>

#include <sihd/sys/env.hpp>
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
    const std::optional<std::string> path = env::get("PATH");
    if (!path.has_value())
        return false;

    Splitter splitter(":");
    for (const std::string & subpath : splitter.split(*path))
    {
        if (fs::is_executable(fs::combine(subpath, binary_name)))
            return true;
    }

    return false;
}

bool is_run_by_valgrind()
{
    const std::optional<std::string> ldpreload = env::get("LD_PRELOAD");
    return ldpreload.has_value()
           && (strstr(ldpreload->c_str(), "/valgrind/") != nullptr
               || strstr(ldpreload->c_str(), "/vgpreload") != nullptr);
}

bool is_run_by_qemu()
{
    // qemu-user passes its own QEMU_LD_PREFIX into the guest environment (cross sysroot).
    // No reliable non-env signal: qemu-11 emulates the vDSO and fakes uname/auxv to the guest.
    return env::get("QEMU_LD_PREFIX").has_value();
}

} // namespace sihd::sys::os
