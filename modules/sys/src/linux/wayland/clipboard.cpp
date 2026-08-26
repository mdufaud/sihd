#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/proc.hpp>
#include <sihd/util/Logger.hpp>

#include "../x11_wayland_backends.hpp"

// Wayland clipboard backend - shells out to wl-copy / wl-paste.

namespace sihd::sys::clipboard
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::clipboard");

bool wayland_set_clipboard(std::string_view str)
{
    proc::Options options({.timeout = std::chrono::milliseconds(200)});
    std::vector<std::string> args;
    args.reserve(2);
    args.emplace_back("wl-copy");
    args.emplace_back(str);
    auto exit_code = proc::execute(args, options);
    auto status = exit_code.wait_for(std::chrono::milliseconds(100));
    if (status == std::future_status::timeout)
        return false;
    return exit_code.get() == 0;
}

std::optional<std::string> wayland_get_clipboard()
{
    std::string clipboard;
    proc::Options options(
        {.timeout = std::chrono::milliseconds(200),
         .stdout_callback = [&clipboard](std::string_view stdout_str) { clipboard += stdout_str; }});
    auto exit_code = proc::execute({"wl-paste", "--type", "text"}, options);
    auto status = exit_code.wait_for(std::chrono::milliseconds(100));
    if (status != std::future_status::timeout && exit_code.get() == 0)
    {
        return clipboard;
    }
    return std::nullopt;
}

bool wayland_set_clipboard_image(const Bitmap & bitmap)
{
    // Generate BMP data in memory using Bitmap's method
    std::vector<uint8_t> bmp_data = bitmap.to_bmp_data();
    if (bmp_data.empty())
        return false;

    // Pass BMP data directly via stdin to wl-copy
    std::string stdin_data(reinterpret_cast<const char *>(bmp_data.data()), bmp_data.size());
    proc::Options options;
    options.timeout = std::chrono::milliseconds(500);
    options.to_stdin = std::move(stdin_data);

    auto exit_code = proc::execute({"wl-copy", "--type", "image/bmp"}, options);
    auto status = exit_code.wait_for(std::chrono::milliseconds(500));
    if (status == std::future_status::timeout)
        return false;
    return exit_code.get() == 0;
}

std::optional<Bitmap> wayland_get_clipboard_image()
{
    std::string bmp_data;
    proc::Options options;
    options.timeout = std::chrono::milliseconds(500);
    options.stdout_callback = [&bmp_data](std::string_view data) {
        bmp_data += data;
    };

    // Try to get image as BMP - wl-paste writes to stdout by default
    auto exit_code = proc::execute({"wl-paste", "--type", "image/bmp"}, options);
    auto status = exit_code.wait_for(std::chrono::milliseconds(500));

    if (status == std::future_status::timeout || exit_code.get() != 0 || bmp_data.empty())
        return std::nullopt;

    // Parse BMP directly from memory
    Bitmap bm;
    std::vector<uint8_t> bmp_bytes(bmp_data.begin(), bmp_data.end());
    if (bm.read_bmp_data(bmp_bytes))
        return bm;
    return std::nullopt;
}

} // namespace sihd::sys::clipboard
