#include <windows.h>

#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::signal
{

SIHD_NEW_LOGGER("sihd::sys::signal");

// utilities

std::expected<void, Error> kill(pid_t pid, int sig)
{
    HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (handle == nullptr)
        return std::unexpected(Error(not_found, "could not open process {}: {}", pid, os::last_error_str()));
    const bool success = TerminateProcess(handle, sig);
    if (!success)
    {
        auto error = Error(io_error, "could not terminate process {}: {}", pid, os::last_error_str());
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }
    CloseHandle(handle);
    return {};
}

std::string name(int sig)
{
    return std::to_string(sig);
}

} // namespace sihd::sys::signal
