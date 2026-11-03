#include "../internal/desktop.hpp"

// clang-format off
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
// clang-format on

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include <sihd/sys/desktop.hpp>
#include <sihd/sys/windows/ComInit.hpp>
#include <sihd/sys/windows/registry.hpp>
#include <sihd/util/str.hpp>

#include "internal/com.hpp"
#include "internal/win_error.hpp"

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::Win32Error;

namespace registry = sihd::sys::windows_::registry;

namespace
{

constexpr ZStringView personalize_key = "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr ZStringView light_theme_value = "AppsUseLightTheme";

Result<void> shell_open(const std::wstring & target)
{
    std::optional<windows_::ComInit> com;
    auto initialized = internal::init_com(com);
    SIHD_UNEXPECTED_RETURN(initialized);

    SHELLEXECUTEINFOW info {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = target.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&info) == 0)
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not open: {}", error));
    }
    return {};
}

Result<std::wstring> absolute_windows_path(ZStringView path)
{
    std::error_code error_code;
    const std::filesystem::path absolute = std::filesystem::absolute(str::to_wstr(path), error_code);
    if (error_code)
        return std::unexpected(Error(invalid_argument, "invalid path '{}': {}", path, error_code.message()));
    return std::filesystem::path(absolute).make_preferred().wstring();
}

} // namespace

namespace internal
{

Result<void> desktop_open_uri(ZStringView uri)
{
    return shell_open(str::to_wstr(uri));
}

Result<void> desktop_open_file(ZStringView path)
{
    auto absolute = absolute_windows_path(path);
    SIHD_UNEXPECTED_RETURN(absolute);
    return shell_open(*absolute);
}

Result<void> desktop_reveal(ZStringView path)
{
    auto absolute = absolute_windows_path(path);
    SIHD_UNEXPECTED_RETURN(absolute);
    std::optional<windows_::ComInit> com;
    auto initialized = init_com(com);
    SIHD_UNEXPECTED_RETURN(initialized);

    PIDLIST_ABSOLUTE item = ILCreateFromPathW(absolute->c_str());
    if (item == nullptr)
        return std::unexpected(Error(not_found, "no shell item for '{}'", path));
    const HRESULT shown = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
    ILFree(item);
    if (FAILED(shown))
        return std::unexpected(Error(io_error, "could not reveal '{}': 0x{:08x}", path, static_cast<ULONG>(shown)));
    return {};
}

} // namespace internal

namespace desktop
{

Result<ColorScheme> color_scheme()
{
    registry::Key key;
    // windows writes neither the key nor the value until the user picks a theme, it starts light
    auto opened = key.open(registry::Hive::current_user, std::string(personalize_key));
    if (!opened.has_value() && opened.error().code == not_found)
        return ColorScheme::light;
    SIHD_UNEXPECTED_RETURN(opened);
    auto light = key.u32(std::string(light_theme_value));
    if (!light.has_value() && light.error().code == not_found)
        return ColorScheme::light;
    SIHD_UNEXPECTED_RETURN(light);
    return *light == 0 ? ColorScheme::dark : ColorScheme::light;
}

} // namespace desktop

} // namespace sihd::sys
