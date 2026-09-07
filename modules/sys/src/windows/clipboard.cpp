#include <shlobj.h>
#include <windows.h>
#include <wingdi.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <sihd/sys/clipboard.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Url.hpp>

// Pixel decoding of image content (png, jpeg, ...) is for a future dedicated
// image module: every format is exposed through get_raw() and served through set_raw().
#pragma message(                                                                                                       \
    "clipboard: image formats are exposed as raw bytes only - pixel decoding comes later in a dedicated image module")

namespace sihd::sys::clipboard
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::clipboard");

namespace
{

// The clipboard is a shared resource - other apps and the Clipboard History
// hold it briefly: retry a few times before giving up.
bool open_clipboard()
{
    int tries = 0;
    while (!OpenClipboard(nullptr))
    {
        if (++tries >= 3)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return true;
}

// Converts the utf-16 text held by `object` into an utf-8 string.
std::optional<std::string> utf8_from_unicode_text(HANDLE object)
{
    const WCHAR *buffer = static_cast<const WCHAR *>(GlobalLock(object));
    if (!buffer)
    {
        SIHD_LOG(error, "failed to lock win32 handle");
        return std::nullopt;
    }
    Defer defer_unlock([&object] { GlobalUnlock(object); });

    // -1 converts the whole null-terminated string, terminator included.
    const int size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        SIHD_LOG(error, "failed to convert win32 clipboard to utf-8");
        return std::nullopt;
    }

    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, out.data(), size, nullptr, nullptr);
    return out;
}

// Wraps the CF_DIB held by `object` into a complete BMP file so the bytes
// match what the linux backends serve for image/bmp.
std::optional<ArrByte> bmp_file_from_dib(HANDLE object)
{
    const uint8_t *dib = static_cast<const uint8_t *>(GlobalLock(object));
    if (dib == nullptr)
        return std::nullopt;
    Defer defer_unlock([&object] { GlobalUnlock(object); });

    const SIZE_T dib_size = GlobalSize(object);
    if (dib_size < sizeof(BITMAPINFOHEADER))
        return std::nullopt;

    const BITMAPINFOHEADER *info = reinterpret_cast<const BITMAPINFOHEADER *>(dib);
    if (info->biSize < sizeof(BITMAPINFOHEADER) || info->biSize > dib_size)
        return std::nullopt;
    if (info->biCompression != BI_RGB || (info->biBitCount != 24 && info->biBitCount != 32))
        return std::nullopt;
    // Color table entries ride between the info header and the pixels: the
    // pixel array offset must account for them or the pixels shift.
    const size_t palette_bytes = static_cast<size_t>(info->biClrUsed) * sizeof(RGBQUAD);
    if (info->biSize + palette_bytes > dib_size)
        return std::nullopt;

    BITMAPFILEHEADER file_header;
    memset(&file_header, 0, sizeof(file_header));
    file_header.bfType = 0x4D42; // 'BM'
    file_header.bfSize = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + dib_size);
    // The DIB header may be wider than BITMAPINFOHEADER (BITMAPV4/V5HEADER)
    // and may be followed by a color table.
    file_header.bfOffBits = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + info->biSize + palette_bytes);

    ArrByte out;
    out.reserve(sizeof(BITMAPFILEHEADER) + dib_size);
    out.push_back(reinterpret_cast<const int8_t *>(&file_header), sizeof(file_header));
    out.push_back(reinterpret_cast<const int8_t *>(dib), dib_size);
    return out;
}

// Appends `path` to `out` as a file:// uri line: backslashes become slashes
// and each path segment is percent-encoded with the shared util Url helper.
void append_file_uri(std::string & out, std::string_view path)
{
    out += "file:///";
    size_t segment_start = 0;
    for (size_t i = 0; i <= path.size(); ++i)
    {
        if (i == path.size() || path[i] == '\\')
        {
            out += Url::str_encode(path.substr(segment_start, i - segment_start));
            if (i < path.size())
                out += '/';
            segment_start = i + 1;
        }
    }
    // text/uri-list lines are CRLF terminated (RFC 2483).
    out += "\r\n";
}

// Converts the file list held by a CF_HDROP handle into a text/uri-list
// payload.
std::optional<std::string> uri_list_from_hdrop(HANDLE object)
{
    const DROPFILES *drop = static_cast<const DROPFILES *>(GlobalLock(object));
    if (drop == nullptr)
        return std::nullopt;
    Defer defer_unlock([&object] { GlobalUnlock(object); });

    if (drop->pFiles < sizeof(DROPFILES))
        return std::nullopt;

    const char *base = reinterpret_cast<const char *>(drop) + drop->pFiles;
    std::string out;
    if (drop->fWide)
    {
        const WCHAR *cursor = reinterpret_cast<const WCHAR *>(base);
        while (*cursor != L'\0')
        {
            const std::wstring wide(cursor);
            cursor += wide.size() + 1;

            const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (size <= 0)
                continue;
            std::string path(size - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, path.data(), size, nullptr, nullptr);
            append_file_uri(out, path);
        }
    }
    else
    {
        const char *cursor = base;
        while (*cursor != '\0')
        {
            const std::string ansi(cursor);
            cursor += ansi.size() + 1;

            // Legacy single-byte paths: go through utf-16 to get utf-8.
            const int wide_size = MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, nullptr, 0);
            if (wide_size <= 0)
                continue;
            std::wstring wide(wide_size, L'\0');
            MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, wide.data(), wide_size);

            const int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (utf8_size <= 0)
                continue;
            std::string path(utf8_size - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, path.data(), utf8_size, nullptr, nullptr);
            append_file_uri(out, path);
        }
    }
    if (out.empty())
        return std::nullopt;
    return out;
}

// Raw bytes of the data `format` holds, requires an open clipboard.
std::optional<ArrByte> bytes_from_format(UINT format)
{
    HANDLE object = GetClipboardData(format);
    if (object == nullptr)
        return std::nullopt;
    const uint8_t *bytes = static_cast<const uint8_t *>(GlobalLock(object));
    if (bytes == nullptr)
        return std::nullopt;
    Defer defer_unlock([&object] { GlobalUnlock(object); });

    // win32 gives no payload size: GlobalSize can over-report the allocation.
    const SIZE_T size = GlobalSize(object);
    if (size == 0)
        return std::nullopt;
    ArrByte out;
    if (!out.from_bytes(bytes, size))
        return std::nullopt;
    return out;
}

// The canonical mime type a clipboard format is served as - nullopt for the
// formats we do not expose.
std::optional<std::string> mime_for_format(UINT format)
{
    if (format == CF_UNICODETEXT)
        return std::string(mime::utf8_text);
    if (format == CF_DIB)
        return std::string(mime::bmp_image);
    if (format == CF_HDROP)
        return std::string(mime::uri_list);
    // Registered formats start at 0xC000; other predefined formats are
    // synthesized or private, not worth listing.
    if (format < 0xC000)
        return std::nullopt;
    char name[128];
    if (GetClipboardFormatNameA(format, name, sizeof(name)) == 0)
        return std::nullopt;
    if (strcmp(name, "HTML Format") == 0)
        return std::string(mime::html);
    return std::string(name);
}

ArrByte to_bytes(std::string_view str)
{
    ArrByte out;
    out.from_bytes(str.data(), str.size());
    return out;
}

// Fetches the bytes `format` holds, labeled with `mime_str`. Requires an open
// clipboard.
std::optional<RawContent> fetch_format(UINT format, const std::string & mime_str)
{
    if (format == CF_UNICODETEXT)
    {
        HANDLE object = GetClipboardData(CF_UNICODETEXT);
        if (object == nullptr)
            return std::nullopt;
        auto text = utf8_from_unicode_text(object);
        if (!text.has_value())
            return std::nullopt;
        return RawContent {mime_str, to_bytes(*text)};
    }
    if (format == CF_DIB)
    {
        HANDLE object = GetClipboardData(CF_DIB);
        if (object == nullptr)
            return std::nullopt;
        auto bmp = bmp_file_from_dib(object);
        if (!bmp.has_value())
            return std::nullopt;
        return RawContent {mime_str, std::move(*bmp)};
    }
    if (format == CF_HDROP)
    {
        HANDLE object = GetClipboardData(CF_HDROP);
        if (object == nullptr)
            return std::nullopt;
        auto uri_list_data = uri_list_from_hdrop(object);
        if (!uri_list_data.has_value())
            return std::nullopt;
        return RawContent {mime_str, to_bytes(*uri_list_data)};
    }
    return bytes_from_format(format).and_then(
        [&mime_str](ArrByte && bytes) { return std::optional(RawContent {mime_str, std::move(bytes)}); });
}

// Copies `data` into a moveable global handle the clipboard can take.
HANDLE copy_to_hglobal(ArrByteView data)
{
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, data.size());
    if (mem == nullptr)
        return nullptr;
    void *locked = GlobalLock(mem);
    if (locked == nullptr)
    {
        GlobalFree(mem);
        return nullptr;
    }
    memcpy(locked, data.data(), data.size());
    GlobalUnlock(mem);
    return mem;
}

// Strips the BITMAPFILEHEADER of a complete BMP file, leaving the CF_DIB the
// win32 clipboard expects.
std::optional<std::vector<uint8_t>> dib_from_bmp_file(ArrByteView bmp)
{
    if (bmp.size() < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
        return std::nullopt;

    const BITMAPFILEHEADER *header = reinterpret_cast<const BITMAPFILEHEADER *>(bmp.data());
    if (header->bfType != 0x4D42) // 'BM'
        return std::nullopt;

    const BITMAPINFOHEADER *info = reinterpret_cast<const BITMAPINFOHEADER *>(bmp.data() + sizeof(BITMAPFILEHEADER));
    if (info->biSize < sizeof(BITMAPINFOHEADER))
        return std::nullopt;
    if (info->biCompression != BI_RGB || (info->biBitCount != 24 && info->biBitCount != 32))
        return std::nullopt;
    // The pixel array may sit after a color table: any offset past the info
    // header works, the DIB handed to win32 keeps everything after the file
    // header (info header + color table + pixels).
    if (header->bfOffBits < sizeof(BITMAPFILEHEADER) + info->biSize || header->bfOffBits >= bmp.size())
        return std::nullopt;

    return std::vector<uint8_t>(bmp.data() + sizeof(BITMAPFILEHEADER), bmp.data() + bmp.size());
}

// Builds a CF_HDROP handle from the file:// uris of a text/uri-list payload.
// Returns nullptr when the list holds no file uri.
HANDLE hdrop_from_uri_list(ArrByteView data)
{
    const std::string text(reinterpret_cast<const char *>(data.data()), data.size());

    std::wstring wide_paths;
    size_t line_start = 0;
    while (line_start < text.size())
    {
        size_t line_end = text.find('\n', line_start);
        if (line_end == std::string::npos)
            line_end = text.size();
        std::string_view line(text.data() + line_start, line_end - line_start);
        line_start = line_end + 1;

        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.remove_suffix(1);
        while (!line.empty() && line.front() == ' ')
            line.remove_prefix(1);
        if (line.empty() || line.front() == '#')
            continue;

        constexpr std::string_view file_scheme = "file://";
        if (line.compare(0, file_scheme.size(), file_scheme) != 0)
            continue;
        line.remove_prefix(file_scheme.size());
        // file://authority/path: an authority gives a UNC path, an empty one a
        // local path - and only local windows drives convert, other absolute
        // paths have no windows equivalent.
        const size_t path_slash = line.find('/');
        if (path_slash == std::string_view::npos)
            continue;
        const std::string_view authority = line.substr(0, path_slash);
        const std::string_view path = line.substr(path_slash + 1);

        std::string windows_path;
        if (authority.empty())
        {
            windows_path = Url::str_decode(path);
            // Only local windows drives convert: other absolute paths have no
            // windows equivalent. Checked decoded: some producers escape the
            // drive colon.
            if (windows_path.size() < 2 || windows_path[1] != ':')
                continue;
        }
        else
        {
            windows_path = "\\\\" + Url::str_decode(authority) + Url::str_decode(path);
        }
        std::replace(windows_path.begin(), windows_path.end(), '/', '\\');

        const int size = MultiByteToWideChar(CP_UTF8, 0, windows_path.c_str(), -1, nullptr, 0);
        if (size <= 1)
            continue;
        std::wstring wide(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, windows_path.c_str(), -1, wide.data(), size);
        wide_paths += wide;
        wide_paths += L'\0';
    }
    if (wide_paths.empty())
        return nullptr;

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(DROPFILES) + (wide_paths.size() + 1) * sizeof(WCHAR));
    if (mem == nullptr)
        return nullptr;
    void *locked = GlobalLock(mem);
    if (locked == nullptr)
    {
        GlobalFree(mem);
        return nullptr;
    }
    DROPFILES drop;
    memset(&drop, 0, sizeof(drop));
    drop.pFiles = sizeof(DROPFILES);
    drop.fWide = TRUE;
    memcpy(locked, &drop, sizeof(drop));
    memcpy(static_cast<char *>(locked) + sizeof(DROPFILES),
           wide_paths.c_str(),
           (wide_paths.size() + 1) * sizeof(WCHAR));
    GlobalUnlock(mem);
    return mem;
}

// Builds the win32 handle serving `content` and outputs the clipboard format
// it maps to. Returns nullptr when the content cannot be served.
HANDLE handle_for_content(const RawContentView & content, UINT & out_format)
{
    if (content.mime == mime::utf8_text || content.mime == mime::plain_text)
    {
        if (content.data.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
            return nullptr;

        out_format = CF_UNICODETEXT;
        const char *text = reinterpret_cast<const char *>(content.data.data());
        const int size = MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(content.data.size()), nullptr, 0);
        if (size <= 0)
            return nullptr;

        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (size + 1) * sizeof(WCHAR));
        if (mem == nullptr)
            return nullptr;
        WCHAR *buffer = static_cast<WCHAR *>(GlobalLock(mem));
        if (buffer == nullptr)
        {
            GlobalFree(mem);
            return nullptr;
        }
        MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(content.data.size()), buffer, size);
        buffer[size] = L'\0';
        GlobalUnlock(mem);
        return mem;
    }
    if (content.mime == mime::bmp_image)
    {
        out_format = CF_DIB;
        auto dib = dib_from_bmp_file(content.data);
        if (!dib.has_value())
            return nullptr;
        return copy_to_hglobal(*dib);
    }
    if (content.mime == mime::uri_list)
    {
        out_format = CF_HDROP;
        return hdrop_from_uri_list(content.data);
    }
    if (content.mime == mime::html)
    {
        out_format = RegisterClipboardFormatA("HTML Format");
        if (out_format == 0)
            return nullptr;
        return copy_to_hglobal(content.data);
    }

    // Any other mime type is offered through a registered format of the same
    // name, bytes served as they are.
    out_format = RegisterClipboardFormatA(std::string(content.mime).c_str());
    if (out_format == 0)
        return nullptr;
    return copy_to_hglobal(content.data);
}

} // namespace

std::vector<RawContent> get_raw()
{
    return get_raw({});
}

// Fetches the wanted mime types - every exposed one when the wanted list is
// empty - in `wanted_mimes` order. Requires an open clipboard.
std::vector<RawContent> get_raw(const std::vector<std::string_view> & wanted_mimes)
{
    if (!open_clipboard())
    {
        SIHD_LOG(error, "failed to open win32 clipboard");
        return {};
    }
    Defer defer_close([] { CloseClipboard(); });

    std::vector<RawContent> contents;
    if (wanted_mimes.empty())
    {
        UINT format = 0;
        while ((format = EnumClipboardFormats(format)) != 0)
        {
            auto mime_str = mime_for_format(format);
            if (!mime_str.has_value())
                continue;
            if (auto content = fetch_format(format, *mime_str))
                contents.push_back(std::move(*content));
        }
        return contents;
    }

    for (std::string_view wanted : wanted_mimes)
    {
        // plain_text is served by the same utf-16 format as utf8_text.
        const std::string fetch_key = wanted == mime::plain_text ? std::string(mime::utf8_text) : std::string(wanted);

        UINT format = 0;
        while ((format = EnumClipboardFormats(format)) != 0)
        {
            auto mime_str = mime_for_format(format);
            if (!mime_str.has_value() || *mime_str != fetch_key)
                continue;
            if (auto content = fetch_format(format, std::string(wanted)))
                contents.push_back(std::move(*content));
            break;
        }
    }
    return contents;
}

bool set_raw(const std::vector<RawContentView> & contents)
{
    if (contents.empty())
        return false;

    // All handles are built before EmptyClipboard: when none is buildable the
    // clipboard is left untouched instead of wiping the user's content.
    std::vector<std::pair<UINT, HANDLE>> to_set;
    for (const RawContentView & content : contents)
    {
        UINT format = 0;
        HANDLE object = handle_for_content(content, format);
        if (object == nullptr || format == 0)
        {
            if (object != nullptr)
                GlobalFree(object);
            SIHD_LOG(warning, "could not serve the clipboard mime '{}'", content.mime);
            continue;
        }
        // Several mime types can map to one format: set it once.
        const auto already_there = [format](const std::pair<UINT, HANDLE> & item) {
            return item.first == format;
        };
        if (std::find_if(to_set.begin(), to_set.end(), already_there) != to_set.end())
        {
            GlobalFree(object);
            continue;
        }
        to_set.emplace_back(format, object);
    }
    if (to_set.empty())
    {
        SIHD_LOG(error, "failed to set any win32 clipboard data");
        return false;
    }

    if (!open_clipboard())
    {
        for (const std::pair<UINT, HANDLE> & item : to_set)
            GlobalFree(item.second);
        SIHD_LOG(error, "failed to open win32 clipboard");
        return false;
    }
    Defer defer_close([] { CloseClipboard(); });

    EmptyClipboard();

    bool set_any = false;
    for (const std::pair<UINT, HANDLE> & item : to_set)
    {
        // On success the system owns the handle - it must not be freed.
        if (SetClipboardData(item.first, item.second) == nullptr)
        {
            SIHD_LOG(error, "failed to set the win32 clipboard format {}", item.first);
            GlobalFree(item.second);
            continue;
        }
        set_any = true;
    }
    if (!set_any)
        SIHD_LOG(error, "failed to set any win32 clipboard data");
    return set_any;
}

} // namespace sihd::sys::clipboard
