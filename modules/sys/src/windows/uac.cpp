// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on

#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <sihd/sys/program.hpp>
#include <sihd/sys/windows/ComInit.hpp>
#include <sihd/sys/windows/uac.hpp>

#include "internal/command_line.hpp"
#include "internal/win_error.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::Win32Error;

namespace sihd::sys::windows_::uac
{

Result<void> relaunch_elevated(std::span<const std::string> args)
{
    std::optional<ComInit> com;
    try
    {
        com.emplace();
    }
    catch (const std::runtime_error & error)
    {
        return std::unexpected(Error(io_error, "could not relaunch elevated: {}", error.what()));
    }
    const std::string path = program::path();
    const std::vector<std::string_view> views(args.begin(), args.end());
    const std::string parameters = internal::command_line(views);

    SHELLEXECUTEINFOA info {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.lpVerb = "runas";
    info.lpFile = path.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExA(&info) == 0)
    {
        const Win32Error error = Win32Error::last();
        if (error.value == ERROR_CANCELLED)
            return std::unexpected(Error(permission_denied, "elevation refused by the user"));
        return std::unexpected(Error(error, "could not relaunch elevated: {}", error));
    }
    return {};
}

} // namespace sihd::sys::windows_::uac
