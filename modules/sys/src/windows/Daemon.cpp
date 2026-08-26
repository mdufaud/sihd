#include <windows.h>

#include <sihd/sys/Daemon.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

bool Daemon::set_user(std::string_view)
{
    SIHD_LOG(error, "Daemon: this platform cannot drop the process to another account");
    return false;
}

bool Daemon::set_user(const user::UserId &)
{
    SIHD_LOG(error, "Daemon: this platform cannot drop the process to another account");
    return false;
}

bool Daemon::set_group(std::string_view)
{
    SIHD_LOG(error, "Daemon: this platform cannot drop the process to another account");
    return false;
}

bool Daemon::set_group(const user::GroupId &)
{
    SIHD_LOG(error, "Daemon: this platform cannot drop the process to another account");
    return false;
}

bool Daemon::run()
{
    // Note: Windows doesn't have fork/setsid like Unix.
    // For a true Windows daemon, use Windows Services (SC API).
    // This implementation provides a simple "detached console" mode.

    // Lock file
    if (_lock_pid_file() == false)
        return false;

    // Install signal handlers
    this->_handle_signals();

    // Detach from console
    if (!FreeConsole())
    {
        // Not an error if there's no console attached
        DWORD err = GetLastError();
        if (err != ERROR_INVALID_PARAMETER) // ERROR_INVALID_PARAMETER = no console
        {
            SIHD_LOG(warning, "Daemon: FreeConsole failed: {}", os::last_error_str());
        }
    }

    // Change directory
    if (!SetCurrentDirectoryA(_working_dir_path.c_str()))
    {
        SIHD_LOG(error, "Daemon: SetCurrentDirectory failed: {}", os::last_error_str());
        return false;
    }

    // Write pid file
    _write_pid_file();

    // Redirect standard handles to NUL
    HANDLE nul_handle = CreateFileA("NUL",
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (nul_handle != INVALID_HANDLE_VALUE)
    {
        SetStdHandle(STD_INPUT_HANDLE, nul_handle);
        SetStdHandle(STD_OUTPUT_HANDLE, nul_handle);
        SetStdHandle(STD_ERROR_HANDLE, nul_handle);
    }
    else
    {
        SIHD_LOG(warning, "Daemon: could not redirect standard handles to NUL");
    }

    SIHD_LOG(info, "Daemon: started with pid: {}", GetCurrentProcessId());
    return true;
}

} // namespace sihd::sys
