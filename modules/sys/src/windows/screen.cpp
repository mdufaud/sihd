#include <windows.h>

#include <string>
#include <vector>

#include <sihd/sys/os.hpp>
#include <sihd/sys/screen.hpp>
#include <sihd/util/Logger.hpp>

#include "../screen_math.hpp"

using enum sihd::util::ErrorCode;

using namespace sihd::util;

namespace sihd::sys::screen
{

SIHD_NEW_LOGGER("sihd::sys::screen");

namespace
{

constexpr int mdt_effective_dpi = 0;

using GetDpiForMonitorFn = HRESULT(WINAPI *)(HMONITOR, int, UINT *, UINT *);

GetDpiForMonitorFn get_dpi_for_monitor()
{
    const HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore == nullptr)
        return nullptr;
    return reinterpret_cast<GetDpiForMonitorFn>(reinterpret_cast<void *>(GetProcAddress(shcore, "GetDpiForMonitor")));
}

std::string narrow_string(const wchar_t *wide)
{
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), size, nullptr, nullptr);
    return out;
}

Monitor assemble(HMONITOR monitor_handle, const MONITORINFOEXW & info)
{
    Monitor monitor;
    monitor.name = narrow_string(info.szDevice);
    monitor.x = info.rcMonitor.left;
    monitor.y = info.rcMonitor.top;
    monitor.width = info.rcMonitor.right - info.rcMonitor.left;
    monitor.height = info.rcMonitor.bottom - info.rcMonitor.top;
    monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

    HDC hdc = CreateDCW(info.szDevice, nullptr, nullptr, nullptr);
    if (hdc != nullptr)
    {
        monitor.width_mm = GetDeviceCaps(hdc, HORZSIZE);
        monitor.height_mm = GetDeviceCaps(hdc, VERTSIZE);
        UINT dpi_x = 0;
        UINT dpi_y = 0;
        // Pre 8.1 systems have no shcore: the device context reports the system dpi instead
        static const GetDpiForMonitorFn dpi_fn = get_dpi_for_monitor();
        if (dpi_fn == nullptr || dpi_fn(monitor_handle, mdt_effective_dpi, &dpi_x, &dpi_y) != S_OK)
            dpi_x = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX));
        monitor.dpi = static_cast<double>(dpi_x);
        DeleteDC(hdc);
    }
    else
    {
        monitor.dpi = internal::dpi_from(monitor.width, monitor.width_mm);
    }
    monitor.scale = monitor.dpi > 0.0 ? monitor.dpi / 96.0 : 1.0;

    return monitor;
}

} // namespace

bool supported()
{
    return true;
}

Result<std::vector<Monitor>> monitors()
{
    std::vector<Monitor> monitors;
    const BOOL enumerated = EnumDisplayMonitors(
        nullptr,
        nullptr,
        [](HMONITOR handle, HDC, LPRECT, LPARAM lparam) -> BOOL {
            auto *out = reinterpret_cast<std::vector<Monitor> *>(lparam);
            MONITORINFOEXW info = {};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(handle, &info) == FALSE)
                return TRUE;
            out->push_back(assemble(handle, info));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&monitors));
    if (enumerated == FALSE && monitors.empty())
        return std::unexpected(Error(io_error, "could not enumerate the monitors: {}", os::last_error_str()));
    if (monitors.empty())
        return std::unexpected(Error(not_supported, "no monitor enumerated"));
    return monitors;
}

} // namespace sihd::sys::screen
