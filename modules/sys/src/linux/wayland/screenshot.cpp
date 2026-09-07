#include "screenshot.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <wayland-client.h>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/proc.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "../internal/deadline.hpp"
#include "protocols/wlr-screencopy-unstable-v1-client-protocol.h"

// Native wlr-screencopy capture, with grim / spectacle / gnome-screenshot as
// the tools fallback: compositors exposing no capture protocol (KWin, Mutter)
// only serve screenshots through their own tools.

// linux/memfd.h is a kernel uapi header some libcs do not ship.
#ifndef MFD_CLOEXEC
# define MFD_CLOEXEC 0x0001U
#endif

namespace sihd::sys::screenshot::wayland
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screenshot");

namespace
{

constexpr Duration capture_timeout = time::sec(2);

// Event loops

class Connection;

// Dispatches wayland events until `stop` returns true, the deadline expires or
// the connection fails.
bool dispatch_until(Connection & conn, Timestamp deadline, const std::function<bool()> & stop);

// Bounded roundtrip: dispatches until the compositor acknowledges with the
// sync callback, or the deadline expires.
bool roundtrip_bounded(Connection & conn, Timestamp deadline);

// Connection and registry

// RAII connection binding the screencopy manager, wl_shm and the outputs.
// Listeners are const instance members fed with `this`.
class Connection
{
    public:
        explicit Connection(Timestamp deadline);

        ~Connection()
        {
            if (_registry != nullptr)
                wl_registry_destroy(_registry);
            if (_display != nullptr)
                wl_display_disconnect(_display);
        }

        Connection(const Connection &) = delete;
        Connection & operator=(const Connection &) = delete;

        bool ok() const { return _ok; }

        wl_display *display() { return _display; }

        int fd() const { return _fd; }

        // Globals bound from the registry: the capture manager, the shm
        // allocator and every output.
        zwlr_screencopy_manager_v1 *manager = nullptr;
        wl_shm *shm = nullptr;
        std::vector<wl_output *> outputs;

        // The output whose wl_output.name (v4) is `name`.
        wl_output *output(const std::string & name) const
        {
            for (const auto & [wl_out, output_name] : _output_names)
            {
                if (output_name == name)
                    return wl_out;
            }
            return nullptr;
        }

    private:
        void bind_global(wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
        {
            if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0 && manager == nullptr)
            {
                manager = static_cast<zwlr_screencopy_manager_v1 *>(
                    wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, 1));
            }
            else if (strcmp(interface, wl_shm_interface.name) == 0 && shm == nullptr)
            {
                shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
            }
            else if (strcmp(interface, wl_output_interface.name) == 0)
            {
                // Version 4 announces the output names this backend matches on.
                wl_output *bound = static_cast<wl_output *>(
                    wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 4u)));
                outputs.push_back(bound);
                wl_output_add_listener(bound, &_output_listener, this);
            }
        }

        const wl_registry_listener _registry_listener = {
            .global =
                [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
                    static_cast<Connection *>(data)->bind_global(registry, name, interface, version);
                },
            .global_remove = [](void *, wl_registry *, uint32_t) {},
        };

        const wl_output_listener _output_listener = {
            .geometry = [](void *,
                           wl_output *,
                           int32_t,
                           int32_t,
                           int32_t,
                           int32_t,
                           int32_t,
                           const char *,
                           const char *,
                           int32_t) {},
            .mode = [](void *, wl_output *, uint32_t, int32_t, int32_t, int32_t) {},
            .done = [](void *, wl_output *) {},
            .scale = [](void *, wl_output *, int32_t) {},
            .name = [](void *data,
                       wl_output *output,
                       const char *name) { static_cast<Connection *>(data)->_output_names[output] = name; },
            .description = [](void *, wl_output *, const char *) {},
        };

        std::map<wl_output *, std::string> _output_names;
        wl_display *_display = nullptr;
        wl_registry *_registry = nullptr;
        int _fd = -1;
        bool _ok = false;
};

Connection::Connection(Timestamp deadline)
{
    _display = wl_display_connect(nullptr);
    if (_display == nullptr)
        return;
    _fd = wl_display_get_fd(_display);
    _registry = wl_display_get_registry(_display);
    wl_registry_add_listener(_registry, &_registry_listener, this);
    // The first roundtrip binds the globals, the second lets the outputs
    // announce their names - bounded: a wedged compositor must not hang the
    // call.
    if (!roundtrip_bounded(*this, deadline) || !roundtrip_bounded(*this, deadline))
        return;
    _ok = manager != nullptr && shm != nullptr;
}

// Dispatches wayland events until `stop` returns true, the deadline expires or
// the connection fails.
bool dispatch_until(Connection & conn, Timestamp deadline, const std::function<bool()> & stop)
{
    wl_display *display = conn.display();
    Poll poll {1};
    poll.set_read_fd(conn.fd());

    wl_display_flush(display);
    while (!stop())
    {
        if (wl_display_get_error(display) != 0)
            return false;
        if (wl_display_prepare_read(display) != 0)
        {
            // Events are already queued: dispatch them without reading.
            if (wl_display_dispatch_pending(display) < 0)
                return false;
            continue;
        }

        const int ret = poll.poll(internal::poll_timeout_ms(deadline));
        if (ret <= 0)
        {
            wl_display_cancel_read(display);
            // A polling error, or the deadline reached, gives up.
            if (poll.polling_error() || internal::steady_now() >= deadline)
                return false;
            // Signal interrupt while time is left: keep waiting.
            continue;
        }
        if (wl_display_read_events(display) < 0)
            return false;
        if (wl_display_dispatch_pending(display) < 0)
            return false;
    }
    return true;
}

// Bounded roundtrip: dispatches until the compositor acknowledges with the
// sync callback, or the deadline expires.
bool roundtrip_bounded(Connection & conn, Timestamp deadline)
{
    struct SyncDone
    {
            bool done = false;
    };
    SyncDone done;
    const wl_callback_listener listener = {
        .done = [](void *data, wl_callback *, uint32_t) { static_cast<SyncDone *>(data)->done = true; },
    };
    wl_callback *callback = wl_display_sync(conn.display());
    if (callback == nullptr)
        return false;
    wl_callback_add_listener(callback, &listener, &done);
    const bool ok = dispatch_until(conn, deadline, [&] { return done.done; });
    wl_callback_destroy(callback);
    return ok;
}

// Capture

// One capture_output round: the frame announces the shm parameters, the
// matching buffer is created and copied into - `ready` means the pixels are
// in the mapping, `failed` gives up.
class Capture
{
    public:
        Capture(Connection & conn, wl_output *output): _conn(conn)
        {
            _frame = zwlr_screencopy_manager_v1_capture_output(conn.manager, 0, output);
            if (_frame != nullptr)
                zwlr_screencopy_frame_v1_add_listener(_frame, &_frame_listener, this);
        }

        ~Capture()
        {
            if (_buffer != nullptr)
                wl_buffer_destroy(_buffer);
            if (_pool != nullptr)
                wl_shm_pool_destroy(_pool);
            if (_map != nullptr)
                munmap(_map, _map_size);
            if (_fd >= 0)
                close(_fd);
            if (_frame != nullptr)
                zwlr_screencopy_frame_v1_destroy(_frame);
        }

        Capture(const Capture &) = delete;
        Capture & operator=(const Capture &) = delete;

        // Dispatches until the copy landed, failed or the deadline expired.
        bool run(Timestamp deadline)
        {
            return _frame != nullptr && dispatch_until(_conn, deadline, [&] { return _done || _failed || _broken; })
                   && _done;
        }

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t stride = 0;
        uint32_t format = 0;
        bool y_invert = false;

        const uint8_t *pixels() const { return static_cast<const uint8_t *>(_map); }

    private:
        void on_buffer(uint32_t buffer_format, uint32_t buffer_width, uint32_t buffer_height, uint32_t buffer_stride)
        {
            format = buffer_format;
            width = buffer_width;
            height = buffer_height;
            stride = buffer_stride;
            this->create_shm_buffer();
        }

        // memfd backed wl_shm pool sized by the announced stride, then the
        // copy request.
        void create_shm_buffer()
        {
            const size_t map_size = static_cast<size_t>(stride) * height;
            if (width == 0 || height == 0 || stride < static_cast<size_t>(width) * 4
                || map_size > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
            {
                _broken = true;
                return;
            }
            _fd = memfd_create("sihd-screenshot", MFD_CLOEXEC);
            if (_fd < 0 || ftruncate(_fd, static_cast<off_t>(map_size)) != 0)
            {
                _broken = true;
                return;
            }
            _map = mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
            if (_map == MAP_FAILED)
            {
                _map = nullptr;
                _broken = true;
                return;
            }
            _map_size = map_size;
            _pool = wl_shm_create_pool(_conn.shm, _fd, static_cast<int32_t>(map_size));
            if (_pool == nullptr)
            {
                _broken = true;
                return;
            }
            _buffer = wl_shm_pool_create_buffer(_pool,
                                                0,
                                                static_cast<int32_t>(width),
                                                static_cast<int32_t>(height),
                                                static_cast<int32_t>(stride),
                                                format);
            if (_buffer == nullptr)
            {
                _broken = true;
                return;
            }
            zwlr_screencopy_frame_v1_copy(_frame, _buffer);
            // The copy request was queued from within a listener: it only
            // reaches the compositor on an explicit flush.
            wl_display_flush(_conn.display());
        }

        const zwlr_screencopy_frame_v1_listener _frame_listener = {
            .buffer =
                [](void *data,
                   zwlr_screencopy_frame_v1 *,
                   uint32_t buffer_format,
                   uint32_t buffer_width,
                   uint32_t buffer_height,
                   uint32_t buffer_stride) {
                    static_cast<Capture *>(data)->on_buffer(buffer_format, buffer_width, buffer_height, buffer_stride);
                },
            .flags =
                [](void *data, zwlr_screencopy_frame_v1 *, uint32_t flags) {
                    static_cast<Capture *>(data)->y_invert = (flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) != 0;
                },
            .ready =
                [](void *data, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t) {
                    static_cast<Capture *>(data)->_done = true;
                },
            .failed = [](void *data, zwlr_screencopy_frame_v1 *) { static_cast<Capture *>(data)->_failed = true; },
            .damage = [](void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t, uint32_t) {},
            .linux_dmabuf = [](void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t) {},
            .buffer_done = [](void *, zwlr_screencopy_frame_v1 *) {},
        };

        Connection & _conn;
        zwlr_screencopy_frame_v1 *_frame = nullptr;
        wl_shm_pool *_pool = nullptr;
        wl_buffer *_buffer = nullptr;
        void *_map = nullptr;
        size_t _map_size = 0;
        int _fd = -1;
        bool _done = false;
        bool _failed = false;
        bool _broken = false;
};

// Pixel decoding

// One wl_shm capture line into the Bitmap B,G,R,A memory order: channel
// shifts per fourcc family, alpha forced on the X-variants.
bool fill_bitmap(const Capture & capture, Bitmap & bm)
{
    if (capture.pixels() == nullptr || capture.width == 0 || capture.height == 0)
        return false;

    uint32_t r_shift = 0, g_shift = 0, b_shift = 0, a_shift = 0;
    bool has_alpha = false;
    switch (capture.format)
    {
        case WL_SHM_FORMAT_XRGB8888:
        case WL_SHM_FORMAT_ARGB8888:
            r_shift = 16, g_shift = 8, b_shift = 0, a_shift = 24;
            has_alpha = capture.format == WL_SHM_FORMAT_ARGB8888;
            break;
        case WL_SHM_FORMAT_XBGR8888:
        case WL_SHM_FORMAT_ABGR8888:
            r_shift = 0, g_shift = 8, b_shift = 16, a_shift = 24;
            has_alpha = capture.format == WL_SHM_FORMAT_ABGR8888;
            break;
        case WL_SHM_FORMAT_RGBX8888:
        case WL_SHM_FORMAT_RGBA8888:
            r_shift = 24, g_shift = 16, b_shift = 8, a_shift = 0;
            has_alpha = capture.format == WL_SHM_FORMAT_RGBA8888;
            break;
        case WL_SHM_FORMAT_BGRX8888:
        case WL_SHM_FORMAT_BGRA8888:
            r_shift = 8, g_shift = 16, b_shift = 24, a_shift = 0;
            has_alpha = capture.format == WL_SHM_FORMAT_BGRA8888;
            break;
        default:
            SIHD_LOG(error, "wayland: unhandled shm pixel format {}", capture.format);
            return false;
    }

    const size_t pixel_count = static_cast<size_t>(capture.width) * capture.height;
    std::vector<uint8_t> bgra(pixel_count * 4);
    for (uint32_t line = 0; line < capture.height; ++line)
    {
        // y_invert: the compositor copied the screen bottom-up.
        const uint32_t src_line = capture.y_invert ? (capture.height - 1 - line) : line;
        const uint8_t *src = capture.pixels() + static_cast<size_t>(src_line) * capture.stride;
        uint8_t *dst = bgra.data() + static_cast<size_t>(line) * capture.width * 4;
        for (uint32_t col = 0; col < capture.width; ++col)
        {
            uint32_t px = 0;
            memcpy(&px, src + static_cast<size_t>(col) * 4, sizeof(px));
            dst[col * 4 + 0] = static_cast<uint8_t>((px >> b_shift) & 0xff);
            dst[col * 4 + 1] = static_cast<uint8_t>((px >> g_shift) & 0xff);
            dst[col * 4 + 2] = static_cast<uint8_t>((px >> r_shift) & 0xff);
            dst[col * 4 + 3] = static_cast<uint8_t>(has_alpha ? ((px >> a_shift) & 0xff) : 0xff);
        }
    }
    bm.create(capture.width, capture.height, 32);
    bm.set(bgra.data(), bgra.size());
    return true;
}

// Tools fallback

// Reads the next ppm header token: whitespace separated, #-comments to eol.
bool ppm_token(std::istream & in, std::string & out)
{
    out.clear();
    char c = 0;
    while (in.get(c))
    {
        if (c == '#')
        {
            while (in.get(c) && c != '\n')
            {
            }
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            if (!out.empty())
                return true;
            continue;
        }
        out.push_back(c);
    }
    return !out.empty();
}

// grim -t ppm writes binary P6: "P6" magic, width, height, 255 maxval, then
// RGB rows.
bool read_ppm_into(const std::string & path, Bitmap & bm)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        return false;

    std::string token;
    if (!ppm_token(in, token) || token != "P6")
        return false;
    if (!ppm_token(in, token))
        return false;
    const size_t width = std::strtoul(token.c_str(), nullptr, 10);
    if (!ppm_token(in, token))
        return false;
    const size_t height = std::strtoul(token.c_str(), nullptr, 10);
    if (!ppm_token(in, token) || std::strtoul(token.c_str(), nullptr, 10) != 255)
    {
        SIHD_LOG(error, "wayland: unsupported ppm maxval '{}' in {}", token, path);
        return false;
    }
    if (width == 0 || height == 0 || width > (std::numeric_limits<size_t>::max)() / (height * 3))
        return false;

    std::vector<uint8_t> rgb(width * height * 3);
    in.read(reinterpret_cast<char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    if (static_cast<size_t>(in.gcount()) != rgb.size())
        return false;

    std::vector<uint8_t> bgra(width * height * 4);
    for (size_t i = 0; i < width * height; ++i)
    {
        bgra[i * 4 + 0] = rgb[i * 3 + 2];
        bgra[i * 4 + 1] = rgb[i * 3 + 1];
        bgra[i * 4 + 2] = rgb[i * 3 + 0];
        bgra[i * 4 + 3] = 0xff;
    }
    bm.create(width, height, 32);
    bm.set(bgra.data(), bgra.size());
    return true;
}

struct ToolCall
{
        std::vector<std::string> args;
        std::string path;
        bool ppm_output = false;
};

bool run_tool(const ToolCall & tool, Bitmap & bm)
{
    proc::Options options;
    options.timeout = std::chrono::seconds(5);
    options.close_stderr = true;

    auto exit_code_future = proc::execute(tool.args, options);
    if (exit_code_future.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
    {
        SIHD_LOG(debug, "wayland: screenshot tool '{}' timed out", tool.args.front());
        return false;
    }
    const int exit_code = exit_code_future.get();
    if (exit_code != 0)
    {
        SIHD_LOG(debug, "wayland: screenshot tool '{}' failed (exit {})", tool.args.front(), exit_code);
        return false;
    }

    if (!fs::is_file(tool.path))
    {
        SIHD_LOG(debug, "wayland: screenshot tool '{}' wrote no file", tool.args.front());
        return false;
    }
    Defer remove_file([&] { fs::remove_file(tool.path); });
    return tool.ppm_output ? read_ppm_into(tool.path, bm) : bm.read_bmp(tool.path);
}

bool take_screen_native(Bitmap & bm, const std::string & output_name)
{
    const Timestamp deadline = internal::deadline_from_now(capture_timeout);
    Connection conn(deadline);
    if (!conn.ok())
        return false;
    // Expected where no capture protocol exists (KWin, Mutter): tools take over.
    if (conn.manager == nullptr)
        return false;

    const auto capture_into = [&](wl_output *output) {
        Capture capture(conn, output);
        if (!capture.run(deadline))
        {
            SIHD_LOG(error, "wayland: screencopy capture failed");
            return false;
        }
        return fill_bitmap(capture, bm);
    };

    if (!output_name.empty())
    {
        wl_output *output = conn.output(output_name);
        if (output == nullptr)
        {
            SIHD_LOG(debug, "wayland: no output named '{}'", output_name);
            return false;
        }
        return capture_into(output);
    }
    // A multi-output full screen is left to the tools: grim stitches every
    // output, the protocol captures one at a time.
    if (conn.outputs.size() > 1)
    {
        SIHD_LOG(debug, "wayland: {} outputs, native capture takes one - trying tools first", conn.outputs.size());
        return false;
    }
    if (conn.outputs.empty())
        return false;

    return capture_into(conn.outputs.front());
}

bool take_screen_binaries(Bitmap & bm, const std::string & output_name)
{
    const std::string tmp_dir = fs::tmp_path();

    std::vector<std::string> grim_args = {"grim", "-t", "ppm"};
    if (!output_name.empty())
    {
        grim_args.emplace_back("-o");
        grim_args.emplace_back(output_name);
    }
    grim_args.emplace_back(tmp_dir + "/sihd_screenshot.ppm");

    const std::vector<ToolCall> tools = {
        // grim (wlroots compositors): ppm output avoids an image decoder
        {grim_args, tmp_dir + "/sihd_screenshot.ppm", true},
        // spectacle (KDE Plasma), then gnome-screenshot (GNOME)
        {{"spectacle", "-b", "-n", "-f", "-o", tmp_dir + "/sihd_screenshot.bmp"},
         tmp_dir + "/sihd_screenshot.bmp",
         false},
        {{"gnome-screenshot", "-f", tmp_dir + "/sihd_screenshot.bmp"}, tmp_dir + "/sihd_screenshot.bmp", false},
    };

    for (const ToolCall & tool : tools)
    {
        if (run_tool(tool, bm))
            return true;
    }

    SIHD_LOG(error, "wayland: no working screenshot tool (tried: grim, spectacle, gnome-screenshot)");
    return false;
}

} // namespace

bool take_screen(Bitmap & bm, const std::string & output_name)
{
    return take_screen_native(bm, output_name) || take_screen_binaries(bm, output_name);
}

bool take_focused(Bitmap & bm)
{
    // spectacle -a and gnome-screenshot -w know the focused window: the
    // capture protocols expose no window geometry.
    const std::string bmp_path = fs::tmp_path() + "/sihd_screenshot.bmp";
    const std::vector<ToolCall> tools = {
        {{"spectacle", "-b", "-n", "-a", "-o", bmp_path}, bmp_path, false},
        {{"gnome-screenshot", "-w", "-f", bmp_path}, bmp_path, false},
    };
    for (const ToolCall & tool : tools)
    {
        if (run_tool(tool, bm))
            return true;
    }

    SIHD_LOG(debug, "wayland: no focused-window capture tool, falling back to full screen");
    return take_screen(bm);
}

bool take_under_cursor(Bitmap & bm)
{
    // No protocol exposes the cursor position: full screen.
    return take_screen(bm);
}

bool take_window_name([[maybe_unused]] Bitmap & bm, [[maybe_unused]] std::string_view name)
{
    // Wayland does not expose the window list to clients.
    SIHD_LOG(error, "wayland: window name capture not supported");
    return false;
}

} // namespace sihd::sys::screenshot::wayland
