#include <sihd/sys/clipboard.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include <windows.h>
#include <wingdi.h>

namespace sihd::sys::clipboard
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::clipboard");

namespace
{

bool windows_set_clipboard(std::string_view str)
{
    int characterCount;
    HANDLE object;
    WCHAR *buffer;

    characterCount = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, NULL, 0);
    if (!characterCount)
        return false;

    object = GlobalAlloc(GMEM_MOVEABLE, characterCount * sizeof(WCHAR));
    if (!object)
    {
        SIHD_LOG(error, "failed to allocate clipboard win32 handle");
        return false;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer)
    {
        SIHD_LOG(error, "failed to lock win32 handle");
        GlobalFree(object);
        return false;
    }

    MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, buffer, characterCount);
    GlobalUnlock(object);

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    int tries = 0;
    while (!OpenClipboard(0))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        tries++;

        if (tries == 3)
        {
            SIHD_LOG(error, "failed to open win32 clipboard");
            GlobalFree(object);
            return false;
        }
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, object);
    CloseClipboard();

    return true;
}

std::optional<std::string> windows_get_clipboard()
{
    HANDLE object;
    WCHAR *buffer;

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    int tries = 0;
    while (!OpenClipboard(0))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        tries++;

        if (tries == 3)
        {
            SIHD_LOG(error, "failed to open win32 clipboard");
            return std::nullopt;
        }
    }

    object = GetClipboardData(CF_UNICODETEXT);
    if (!object)
    {
        SIHD_LOG(error, "failed to convert win32 clipboard to string");
        CloseClipboard();
        return std::nullopt;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer)
    {
        SIHD_LOG(error, "failed to lock win32 handle");
        CloseClipboard();
        return std::nullopt;
    }

    std::wstring ws(buffer);

    GlobalUnlock(object);
    CloseClipboard();

    return std::string(ws.begin(), ws.end());
}

// ============================================================================
// Image clipboard functions
// ============================================================================

bool windows_set_clipboard_image(const Bitmap & bitmap)
{
    if (bitmap.empty())
        return false;

    const size_t width = bitmap.width();
    const size_t height = bitmap.height();
    const size_t bytes_per_row = width * bitmap.byte_per_pixel();
    const size_t padded_row_size = ((bytes_per_row + 3) / 4) * 4;
    const size_t image_size = padded_row_size * height;

    // Create DIB header
    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = static_cast<LONG>(width);
    bi.biHeight = static_cast<LONG>(height); // positive = bottom-up
    bi.biPlanes = 1;
    bi.biBitCount = bitmap.byte_per_pixel() * 8;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = static_cast<DWORD>(image_size);

    // Allocate global memory for DIB
    size_t total_size = sizeof(BITMAPINFOHEADER) + image_size;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, total_size);
    if (!hMem)
    {
        SIHD_LOG(error, "failed to allocate clipboard memory for image");
        return false;
    }

    uint8_t *pMem = static_cast<uint8_t *>(GlobalLock(hMem));
    if (!pMem)
    {
        GlobalFree(hMem);
        return false;
    }

    // Copy header
    memcpy(pMem, &bi, sizeof(BITMAPINFOHEADER));

    // Copy pixel data with padding
    uint8_t *pPixels = pMem + sizeof(BITMAPINFOHEADER);
    const size_t src_row_size = width * bitmap.byte_per_pixel();
    for (size_t y = 0; y < height; ++y)
    {
        memcpy(pPixels + y * padded_row_size, bitmap.c_data() + y * src_row_size, src_row_size);
        // Padding bytes are already zero from GlobalAlloc
    }

    GlobalUnlock(hMem);

    // Open clipboard
    int tries = 0;
    while (!OpenClipboard(nullptr))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (++tries >= 3)
        {
            SIHD_LOG(error, "failed to open clipboard for image");
            GlobalFree(hMem);
            return false;
        }
    }

    EmptyClipboard();
    HANDLE result = SetClipboardData(CF_DIB, hMem);
    CloseClipboard();

    if (!result)
    {
        GlobalFree(hMem);
        return false;
    }

    return true;
}

std::optional<Bitmap> windows_get_clipboard_image()
{
    int tries = 0;
    while (!OpenClipboard(nullptr))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (++tries >= 3)
        {
            SIHD_LOG(error, "failed to open clipboard for image");
            return std::nullopt;
        }
    }

    Defer defer_close([] { CloseClipboard(); });

    // Try CF_DIB first (device-independent bitmap)
    HANDLE hData = GetClipboardData(CF_DIB);
    if (!hData)
        return std::nullopt;

    const BITMAPINFOHEADER *pBIH = static_cast<const BITMAPINFOHEADER *>(GlobalLock(hData));
    if (!pBIH)
        return std::nullopt;

    Defer defer_unlock([&hData] { GlobalUnlock(hData); });

    if (pBIH->biBitCount != 24 && pBIH->biBitCount != 32)
    {
        SIHD_LOG(warning, "Clipboard image has unsupported bit depth: {}", pBIH->biBitCount);
        return std::nullopt;
    }

    const size_t width = pBIH->biWidth;
    const size_t height = std::abs(pBIH->biHeight);
    const bool top_down = pBIH->biHeight < 0;
    const size_t bpp = pBIH->biBitCount / 8;

    Bitmap bm;
    bm.create(width, height, pBIH->biBitCount);

    // Calculate row sizes
    const size_t src_row_size = ((width * bpp + 3) / 4) * 4; // padded
    const size_t dst_row_size = width * bpp;

    const uint8_t *pPixels = reinterpret_cast<const uint8_t *>(pBIH) + sizeof(BITMAPINFOHEADER);

    // Handle color table for <= 8bpp (not implemented, we only support 24/32)

    for (size_t y = 0; y < height; ++y)
    {
        const size_t src_y = top_down ? (height - 1 - y) : y;
        const uint8_t *src_row = pPixels + src_y * src_row_size;
        // Use set for each pixel or direct memcpy
        memcpy(const_cast<uint8_t *>(bm.c_data()) + y * dst_row_size, src_row, dst_row_size);
    }

    return bm;
}

} // namespace

bool set_text(std::string_view str)
{
    return windows_set_clipboard(str);
}

bool set_image(const Bitmap & bitmap)
{
    return windows_set_clipboard_image(bitmap);
}

std::optional<std::string> get_text()
{
    return windows_get_clipboard();
}

std::optional<Bitmap> get_image()
{
    return windows_get_clipboard_image();
}

std::optional<Content> get_any()
{
    // Try image first
    if (auto img = get_image(); img.has_value())
        return Content {std::move(*img)};

    // Fall back to text
    if (auto txt = get_text(); txt.has_value())
        return Content {std::move(*txt)};

    return std::nullopt;
}

} // namespace sihd::sys::clipboard
