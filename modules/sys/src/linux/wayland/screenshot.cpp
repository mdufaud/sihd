#include <sihd/sys/proc.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include "../x11_wayland_backends.hpp"

// Wayland screenshot backend - shells out to grim / spectacle / gnome-screenshot.

namespace sihd::sys::screenshot
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screenshot");

bool wayland_screenshot(Bitmap & bm, const std::string & output_name)
{
    proc::Options options;
    options.timeout = std::chrono::seconds(5);
    options.close_stderr = true;
    std::string tmp_path = fs::tmp_path() + "/sihd_screenshot.bmp";

    // Try different screenshot tools in order of preference
    std::vector<std::vector<std::string>> tools;

    // grim (wlroots compositors: Sway, etc.)
    {
        std::vector<std::string> args;
        args.emplace_back("grim");
        args.emplace_back("-t");
        args.emplace_back("bmp");
        if (!output_name.empty())
        {
            args.emplace_back("-o");
            args.emplace_back(output_name);
        }
        args.emplace_back(tmp_path);
        tools.push_back(args);
    }

    // spectacle (KDE Plasma)
    {
        std::vector<std::string> args = {"spectacle", "-b", "-n", "-f", "-o", tmp_path};
        tools.push_back(args);
    }

    // gnome-screenshot (GNOME)
    {
        std::vector<std::string> args = {"gnome-screenshot", "-f", tmp_path};
        tools.push_back(args);
    }

    for (const auto & args : tools)
    {
        auto exit_code_future = proc::execute(args, options);
        auto status = exit_code_future.wait_for(std::chrono::seconds(5));

        if (status == std::future_status::timeout || exit_code_future.get() != 0)
            continue;

        if (fs::is_file(tmp_path))
        {
            Defer defer_cleanup([&] { fs::remove_file(tmp_path); });
            if (bm.read_bmp(tmp_path))
                return true;
        }
    }

    SIHD_LOG(error, "No working screenshot tool found (tried: grim, spectacle, gnome-screenshot)");
    return false;
}

bool wayland_screenshot_focused(Bitmap & bm)
{
    proc::Options options;
    options.timeout = std::chrono::seconds(5);
    options.close_stderr = true;

    std::string tmp_path = fs::tmp_path() + "/sihd_screenshot.bmp";

    // Try tools that support active window capture
    std::vector<std::vector<std::string>> tools = {
        // spectacle (KDE) - active window
        {"spectacle", "-b", "-n", "-a", "-o", tmp_path},
        // gnome-screenshot - current window
        {"gnome-screenshot", "-w", "-f", tmp_path},
    };

    for (const auto & args : tools)
    {
        auto exit_code_future = proc::execute(args, options);
        auto status = exit_code_future.wait_for(std::chrono::seconds(5));

        if (status == std::future_status::timeout || exit_code_future.get() != 0)
            continue;

        if (fs::is_file(tmp_path))
        {
            Defer defer_cleanup([&] { fs::remove_file(tmp_path); });
            if (bm.read_bmp(tmp_path))
                return true;
        }
    }

    SIHD_LOG(warning, "Wayland: focused window capture not supported, falling back to full screen");
    return wayland_screenshot(bm);
}

bool wayland_screenshot_under_cursor(Bitmap & bm)
{
    // spectacle supports interactive region selection with -r but not "under cursor" specifically
    SIHD_LOG(warning, "Wayland: under cursor capture not supported, falling back to full screen");
    return wayland_screenshot(bm);
}

bool wayland_screenshot_window_name([[maybe_unused]] Bitmap & bm, [[maybe_unused]] std::string_view name)
{
    // Wayland doesn't expose window list to clients for security
    SIHD_LOG(error, "Wayland: window name capture not supported");
    return false;
}

} // namespace sihd::sys::screenshot
