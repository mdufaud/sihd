#include <windows.h>

#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys::signal
{

SIHD_NEW_LOGGER("sihd::sys::signal");

// utilities

bool kill(pid_t pid, int sig)
{
    HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (handle == nullptr)
        return false;
    const bool success = TerminateProcess(handle, sig);
    CloseHandle(handle);
    return success;
}

std::string name(int sig)
{
    return std::to_string(sig);
}

} // namespace sihd::sys::signal
