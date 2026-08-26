#include <sihd/sys/screenshot.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include <windows.h>
#include <wingdi.h>
#include <winuser.h>

namespace sihd::sys::screenshot
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screenshot");

namespace
{

bool take_screen_from_window(Bitmap & bm, HWND window)
{
    bool ret = false;
    if (window == nullptr)
        return false;

    HDC hdcScreen = GetDC(window);
    if (hdcScreen == nullptr)
        return false;

    Defer defer_release_dc([&] { ReleaseDC(window, hdcScreen); });

    HDC hdcCompatible = CreateCompatibleDC(hdcScreen);
    if (hdcCompatible == nullptr)
        return false;

    Defer defer_delete_dc([&] { DeleteDC(hdcCompatible); });

    RECT rect;
    if (window == GetDesktopWindow())
    {
        rect.left = 0;
        rect.top = 0;
        rect.right = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    else
    {
        GetClientRect(window, &rect);
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    if (width <= 0 || height <= 0)
        return false;

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
    if (hBmp == nullptr)
        return false;

    Defer defer_delete_bmp([&] { DeleteObject(hBmp); });

    HGDIOBJ hOldBmp = SelectObject(hdcCompatible, hBmp);
    BOOL bOK = BitBlt(hdcCompatible, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY | CAPTUREBLT);

    if (!bOK)
    {
        SelectObject(hdcCompatible, hOldBmp);
        return false;
    }

    SelectObject(hdcCompatible, hOldBmp);

    // Use 32-bit to avoid row padding issues (32-bit rows are always DWORD aligned)
    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = height; // positive = bottom-up DIB (matches our Bitmap storage)
    bi.biPlanes = 1;
    bi.biBitCount = 32; // 32-bit = no padding needed
    bi.biCompression = BI_RGB;
    bi.biSizeImage = width * height * 4;

    std::vector<uint8_t> pixels(bi.biSizeImage);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader = bi;

    int lines = GetDIBits(hdcScreen, hBmp, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);
    if (lines != height)
        return false;

    // Windows returns BGRA, our Pixel struct in little-endian is {blue, green, red, alpha}
    // So the byte order matches - we can copy directly
    bm.create(width, height, 32);
    bm.set(pixels.data(), pixels.size());
    ret = true;

    return ret;
}

} // namespace

bool take_window_name(Bitmap & bm, std::string_view name)
{
    bm.clear();
    HWND active_window = FindWindowA(nullptr, name.data());
    return take_screen_from_window(bm, active_window);
}

bool take_focused(Bitmap & bm)
{
    bm.clear();
    HWND foreground_window = GetForegroundWindow();
    return take_screen_from_window(bm, foreground_window);
}

bool take_under_cursor(Bitmap & bm)
{
    bm.clear();
    POINT pt;
    HWND window;
    GetCursorPos(&pt);
    window = WindowFromPoint(pt);
    return take_screen_from_window(bm, window);
}

bool take_screen(Bitmap & bm)
{
    bm.clear();
    // Use NULL to get DC for the entire virtual screen (all monitors)
    HDC hdcScreen = GetDC(NULL);
    if (hdcScreen == nullptr)
        return false;

    Defer defer_release_dc([&] { ReleaseDC(NULL, hdcScreen); });

    const int width = GetSystemMetrics(SM_CXSCREEN);
    const int height = GetSystemMetrics(SM_CYSCREEN);

    if (width <= 0 || height <= 0)
        return false;

    HDC hdcCompatible = CreateCompatibleDC(hdcScreen);
    if (hdcCompatible == nullptr)
        return false;

    Defer defer_delete_dc([&] { DeleteDC(hdcCompatible); });

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
    if (hBmp == nullptr)
        return false;

    Defer defer_delete_bmp([&] { DeleteObject(hBmp); });

    HGDIOBJ hOldBmp = SelectObject(hdcCompatible, hBmp);
    BOOL bOK = BitBlt(hdcCompatible, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY | CAPTUREBLT);

    if (!bOK)
    {
        SelectObject(hdcCompatible, hOldBmp);
        return false;
    }

    SelectObject(hdcCompatible, hOldBmp);

    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = height;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = width * height * 4;

    std::vector<uint8_t> pixels(bi.biSizeImage);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader = bi;

    int lines = GetDIBits(hdcScreen, hBmp, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);
    if (lines != height)
        return false;

    bm.create(width, height, 32);
    bm.set(pixels.data(), pixels.size());
    return true;
}

} // namespace sihd::sys::screenshot
