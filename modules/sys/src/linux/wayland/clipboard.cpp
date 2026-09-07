#include "clipboard.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <wayland-client.h>

#include <fmt/ranges.h>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include "../internal/deadline.hpp"
#include "protocols/ext-data-control-v1-client-protocol.h"
#include "protocols/wlr-data-control-unstable-v1-client-protocol.h"

// Native wayland clipboard backend: ext_data_control_v1 (KWin, recent wlroots),
// wlr_data_control_v1 (older), core wl_data_device (Mutter). One-shot set.

namespace sihd::sys::clipboard::wayland
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::clipboard");

namespace
{

class Connection;

// Dispatches wayland events until `stop` returns true, the deadline expires or
// the connection fails.
bool dispatch_until(Connection & conn, Timestamp deadline, const std::function<bool()> & stop);

// Connection and registry

// RAII connection binding the clipboard related globals with two roundtrips.
// All its wayland listeners are instance members fed with `this`.
class Connection
{
    public:
        // Binds the clipboard related globals with two roundtrips bounded by
        // `deadline`.
        Connection(Timestamp deadline);

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

        // Globals bound from the registry: data-control managers preferred, the
        // core data device manager is the fallback for compositors without it.
        wl_seat *seat = nullptr;
        ext_data_control_manager_v1 *ext_manager = nullptr;
        zwlr_data_control_manager_v1 *wlr_manager = nullptr;
        wl_data_device_manager *data_device_manager = nullptr;
        // Serial the core wl_data_device.set_selection wants.
        uint32_t keyboard_serial = 0;

    private:
        void bind_global(wl_registry *registry, uint32_t name, const char *interface)
        {
            if (strcmp(interface, wl_seat_interface.name) == 0 && seat == nullptr)
            {
                seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
                wl_seat_add_listener(seat, &_seat_listener, this);
            }
            else if (strcmp(interface, ext_data_control_manager_v1_interface.name) == 0)
            {
                ext_manager = static_cast<ext_data_control_manager_v1 *>(
                    wl_registry_bind(registry, name, &ext_data_control_manager_v1_interface, 1));
            }
            else if (strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0)
            {
                wlr_manager = static_cast<zwlr_data_control_manager_v1 *>(
                    wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 1));
            }
            else if (strcmp(interface, wl_data_device_manager_interface.name) == 0)
            {
                data_device_manager = static_cast<wl_data_device_manager *>(
                    wl_registry_bind(registry, name, &wl_data_device_manager_interface, 1));
            }
        }

        void on_seat_capabilities(wl_seat *seat_proxy, uint32_t capabilities)
        {
            // Serials come from the keyboard: the core wl_data_device
            // set_selection wants one.
            if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) == 0)
                return;

            wl_keyboard *keyboard = wl_seat_get_keyboard(seat_proxy);
            if (keyboard != nullptr)
                wl_keyboard_add_listener(keyboard, &_keyboard_listener, this);
        }

        const wl_registry_listener _registry_listener = {
            .global =
                [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t) {
                    static_cast<Connection *>(data)->bind_global(registry, name, interface);
                },
            .global_remove = [](void *, wl_registry *, uint32_t) {},
        };

        const wl_seat_listener _seat_listener = {
            .capabilities =
                [](void *data, wl_seat *seat_proxy, uint32_t capabilities) {
                    static_cast<Connection *>(data)->on_seat_capabilities(seat_proxy, capabilities);
                },
            .name = [](void *, wl_seat *, const char *) {},
        };

        const wl_keyboard_listener _keyboard_listener = {
            // The keymap fd is ours to close.
            .keymap = [](void *, wl_keyboard *, uint32_t, int32_t fd, uint32_t) { close(fd); },
            .enter =
                [](void *data, wl_keyboard *, uint32_t serial, wl_surface *, wl_array *) {
                    static_cast<Connection *>(data)->keyboard_serial = serial;
                },
            .leave = [](void *, wl_keyboard *, uint32_t, wl_surface *) {},
            .key = [](void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t) {},
            .modifiers = [](void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {},
            .repeat_info = [](void *, wl_keyboard *, int32_t, int32_t) {},
        };

        wl_display *_display = nullptr;
        wl_registry *_registry = nullptr;
        int _fd = -1;
        bool _ok = false;
};

// Event loops and fd pumps

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

Connection::Connection(Timestamp deadline)
{
    _display = wl_display_connect(nullptr);
    if (_display == nullptr)
        return;
    _fd = wl_display_get_fd(_display);
    _registry = wl_display_get_registry(_display);
    wl_registry_add_listener(_registry, &_registry_listener, this);
    // First roundtrip binds the globals, the second lets them emit their
    // initial events (seat capabilities) - bounded like the rest of the flows:
    // a wedged compositor must not hang the call.
    if (!roundtrip_bounded(*this, deadline) || !roundtrip_bounded(*this, deadline))
        return;
    _ok = seat != nullptr;
}

// Reads `fd` until EOF or deadline. Each read is guarded by a poll so a slow
// writer cannot hang us past the deadline.
bool read_fd_until_eof(int fd, Timestamp deadline, ArrByte & out)
{
    Poll poll {1};
    poll.set_read_fd(fd);

    for (;;)
    {
        char buf[4096];
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0)
        {
            out.push_back(reinterpret_cast<const int8_t *>(buf), static_cast<size_t>(n));
            continue;
        }
        if (n == 0)
            return true;
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            return false;
        poll.poll(internal::poll_timeout_ms(deadline));
        if (poll.polling_error() || internal::steady_now() >= deadline)
            return false;
    }
}

// Writes all of `data` to `fd` or gives up at the deadline.
bool write_fd_all(int fd, Timestamp deadline, std::string_view data)
{
    // Non-blocking: on a blocking fd a full pipe hangs the write in the kernel
    // and the deadline never gets a chance to hit.
    if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0)
        return false;
    Poll poll {1};
    poll.set_write_fd(fd);

    size_t written = 0;
    while (written < data.size())
    {
        ssize_t n = ::write(fd, data.data() + written, data.size() - written);
        if (n > 0)
        {
            written += static_cast<size_t>(n);
            continue;
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            return false;
        poll.poll(internal::poll_timeout_ms(deadline));
        if (poll.polling_error() || internal::steady_now() >= deadline)
            return false;
    }
    return true;
}

// Shared clipboard state and serving

// State of one get or set operation, shared by the event listeners. A protocol
// class keeps a reference to it.
struct SelectionState
{
        // get: mime types advertised per offer proxy, and the offer holding the
        // current selection.
        std::map<wl_proxy *, std::vector<std::string>> offer_mimes;
        wl_proxy *selection_offer = nullptr;
        bool selection_seen = false;
        bool finished = false;

        // set: the representations to serve, the completion flags, and the serve
        // deadline shared by the whole set operation. Views over the caller
        // buffers, which outlive the blocking dispatch.
        std::vector<RawContentView> offered;
        Timestamp deadline = {};
        bool served = false;
        bool cancelled = false;
};

// Returns true once the current selection offer is known and its mime types
// were advertised (or the clipboard is empty).
bool selection_offer_ready(const SelectionState & state)
{
    if (!state.selection_seen)
        return false;
    if (state.selection_offer == nullptr)
        return true;
    auto found = state.offer_mimes.find(state.selection_offer);
    return found != state.offer_mimes.end() && !found->second.empty();
}

// Serves a consumer request with the payload matching the requested mime type.
// This is the only place `served` becomes true.
void serve_source_request(SelectionState & state, const char *mime_type, int32_t fd)
{
    std::string_view wanted = mime_type != nullptr ? mime_type : "";

    bool ok = false;
    for (const RawContentView & content : state.offered)
    {
        if (content.mime == wanted)
        {
            ok = write_fd_all(
                fd,
                state.deadline,
                std::string_view(reinterpret_cast<const char *>(content.data.data()), content.data.size()));
            break;
        }
    }
    close(fd);
    state.served = state.served || ok;
}

// Mimes to fetch, in fetch order: every offered one when nothing is wanted,
// else the wanted mimes the offer advertises.
std::vector<std::string> mimes_to_fetch(const std::vector<std::string> & offered,
                                        const std::vector<std::string_view> & wanted_mimes)
{
    if (wanted_mimes.empty())
        return offered;

    std::vector<std::string> fetch_list;
    for (std::string_view mime_str : wanted_mimes)
    {
        // Request only mimes the offer advertises.
        if (auto offered_mime = mime::match_offered(offered, mime_str); offered_mime.has_value())
        {
            if (std::find(fetch_list.begin(), fetch_list.end(), *offered_mime) == fetch_list.end())
                fetch_list.emplace_back(*offered_mime);
        }
    }
    if (fetch_list.empty())
    {
        SIHD_LOG(debug, "wayland: none of the wanted mime types is offered (offered: {})", fmt::join(offered, ", "));
    }
    return fetch_list;
}

// Receives the offered data through a pipe: Derived::receive writes the offer
// fd to the compositor, the mime must be one of the mimes the offer advertises.
template <typename Derived, typename Offer>
std::optional<ArrByte> receive_offer_data(Connection & conn, Timestamp deadline, Offer *offer, const char *mime)
{
    int fds[2];
    if (pipe(fds) != 0)
        return std::nullopt;
    // FD_CLOEXEC keeps the ends out of exec'd children, O_NONBLOCK makes the
    // reads below poll-guarded: both must hold or the deadline cannot be
    // enforced.
    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(fds[1], F_SETFD, FD_CLOEXEC) != 0
        || fcntl(fds[0], F_SETFL, O_NONBLOCK) != 0)
    {
        close(fds[0]);
        close(fds[1]);
        return std::nullopt;
    }
    Defer close_read_end([&] { close(fds[0]); });

    Derived::receive(conn, offer, mime, fds[1]);
    close(fds[1]);
    // The receive request (and its fd) only reach the compositor on flush.
    wl_display_flush(conn.display());

    ArrByte data;
    if (!read_fd_until_eof(fds[0], deadline, data) || data.empty())
        return std::nullopt;
    return data;
}

// Protocol classes

// The ext, wlr and core protocols share the same get and set flows: only their
// types, requests and listener structs differ. The flows are written once here
// over the Derived protocol surface - static request functions - while each
// class keeps its listeners, whose C structs cannot be unified.
template <typename Derived>
class DataControl
{
    public:
        DataControl(Connection & conn, SelectionState & state): _conn(conn), _state(state) {}

        // Fetches the wanted mimes the selection offers in one snapshot:
        // nullopt on protocol failure, empty on an empty clipboard.
        std::optional<std::vector<RawContent>> read(const std::vector<std::string_view> & wanted_mimes,
                                                    Timestamp deadline)
        {
            auto *device = Derived::get_data_device(_conn);
            if (device == nullptr)
                return std::nullopt;
            Defer destroy_device([&] { Derived::destroy_device(device); });
            this->derived().add_device_listener(device);

            // The current selection offer is sent when the device is bound.
            if (!dispatch_until(_conn, deadline, [&] { return selection_offer_ready(_state); }))
                return std::nullopt;
            if (_state.selection_offer == nullptr)
                return std::vector<RawContent>(); // empty clipboard

            auto found = _state.offer_mimes.find(_state.selection_offer);
            if (found == _state.offer_mimes.end())
                return std::nullopt;

            auto *offer = reinterpret_cast<typename Derived::Offer *>(_state.selection_offer);
            Defer destroy_offer([&] { Derived::destroy_offer(offer); });

            std::vector<RawContent> contents;
            for (const std::string & mime_str : mimes_to_fetch(found->second, wanted_mimes))
            {
                auto data = receive_offer_data<Derived>(_conn, deadline, offer, mime_str.c_str());
                if (data.has_value())
                    contents.push_back(RawContent {mime_str, std::move(*data)});
            }
            return contents;
        }

        // Offers the representations, then serves requests until the first
        // consumer is fulfilled, the deadline expires or the source is
        // cancelled.
        bool write(const std::vector<RawContentView> & contents, Timestamp deadline)
        {
            _state.offered = contents;

            auto *source = Derived::create_source(_conn);
            if (source == nullptr)
                return false;
            Defer destroy_source([&] { Derived::destroy_source(source); });
            this->derived().add_source_listener(source);
            for (const RawContentView & content : contents)
            {
                // The offer request takes a C string: the mime views come from
                // users and are not guaranteed null-terminated.
                Derived::offer_source(source, std::string(content.mime).c_str());
            }

            auto *device = Derived::get_data_device(_conn);
            if (device == nullptr)
                return false;
            Defer destroy_device([&] { Derived::destroy_device(device); });
            this->derived().add_device_listener(device);
            Derived::set_selection(_conn, device, source);

            return dispatch_until(_conn, deadline, [&] { return _state.served || _state.cancelled || _state.finished; })
                   && _state.served;
        }

    protected:
        template <typename Offer>
        void on_device_data_offer(Offer *offer)
        {
            _state.offer_mimes[reinterpret_cast<wl_proxy *>(offer)] = {};
            this->derived().add_offer_listener(offer);
        }

        template <typename Offer>
        void on_device_selection(Offer *offer)
        {
            _state.selection_offer = reinterpret_cast<wl_proxy *>(offer);
            _state.selection_seen = true;
        }

        void on_device_finished() { _state.finished = true; }

        template <typename Offer>
        void on_offer_mime(Offer *offer, const char *mime_type)
        {
            _state.offer_mimes[reinterpret_cast<wl_proxy *>(offer)].emplace_back(mime_type);
        }

        void on_source_send(const char *mime_type, int32_t fd) { serve_source_request(_state, mime_type, fd); }

        void on_source_cancelled() { _state.cancelled = true; }

    private:
        Derived & derived() { return static_cast<Derived &>(*this); }

        Connection & _conn;
        SelectionState & _state;
};

// ext_data_control_v1 - spoken by KWin and wlroots >= 0.19 compositors
class ExtDataControl: public DataControl<ExtDataControl>
{
    public:
        using DataControl::DataControl;

        static bool available(const Connection & conn) { return conn.ext_manager != nullptr; }

        using Device = ext_data_control_device_v1;
        using Offer = ext_data_control_offer_v1;
        using Source = ext_data_control_source_v1;

        static Device *get_data_device(const Connection & conn)
        {
            return ext_data_control_manager_v1_get_data_device(conn.ext_manager, conn.seat);
        }
        static void destroy_device(Device *device) { ext_data_control_device_v1_destroy(device); }
        static void destroy_offer(Offer *offer) { ext_data_control_offer_v1_destroy(offer); }
        static Source *create_source(const Connection & conn)
        {
            return ext_data_control_manager_v1_create_data_source(conn.ext_manager);
        }
        static void destroy_source(Source *source) { ext_data_control_source_v1_destroy(source); }
        static void offer_source(Source *source, const char *mime) { ext_data_control_source_v1_offer(source, mime); }
        // The data-control set_selection carries no serial.
        static void set_selection(const Connection &, Device *device, Source *source)
        {
            ext_data_control_device_v1_set_selection(device, source);
        }
        static void receive([[maybe_unused]] Connection & conn, Offer *offer, const char *mime, int fd)
        {
            ext_data_control_offer_v1_receive(offer, mime, fd);
        }

        void add_device_listener(Device *device)
        {
            ext_data_control_device_v1_add_listener(device, &_device_listener, this);
        }
        void add_offer_listener(Offer *offer) { ext_data_control_offer_v1_add_listener(offer, &_offer_listener, this); }
        void add_source_listener(Source *source)
        {
            ext_data_control_source_v1_add_listener(source, &_source_listener, this);
        }

    private:
        const ext_data_control_device_v1_listener _device_listener = {
            .data_offer = [](void *data,
                             Device *,
                             Offer *offer) { static_cast<ExtDataControl *>(data)->on_device_data_offer(offer); },
            .selection = [](void *data,
                            Device *,
                            Offer *offer) { static_cast<ExtDataControl *>(data)->on_device_selection(offer); },
            .finished = [](void *data, Device *) { static_cast<ExtDataControl *>(data)->on_device_finished(); },
            // Primary selection is out of scope.
            .primary_selection = [](void *, Device *, Offer *) {},
        };

        const ext_data_control_source_v1_listener _source_listener = {
            .send =
                [](void *data, Source *, const char *mime_type, int32_t fd) {
                    static_cast<ExtDataControl *>(data)->on_source_send(mime_type, fd);
                },
            .cancelled = [](void *data, Source *) { static_cast<ExtDataControl *>(data)->on_source_cancelled(); },
        };

        const ext_data_control_offer_v1_listener _offer_listener = {
            .offer =
                [](void *data, Offer *offer, const char *mime_type) {
                    static_cast<ExtDataControl *>(data)->on_offer_mime(offer, mime_type);
                },
        };
};

// wlr_data_control_unstable_v1 - spoken by wlroots <= 0.18 era compositors
class WlrDataControl: public DataControl<WlrDataControl>
{
    public:
        using DataControl::DataControl;

        static bool available(const Connection & conn) { return conn.wlr_manager != nullptr; }

        using Device = zwlr_data_control_device_v1;
        using Offer = zwlr_data_control_offer_v1;
        using Source = zwlr_data_control_source_v1;

        static Device *get_data_device(const Connection & conn)
        {
            return zwlr_data_control_manager_v1_get_data_device(conn.wlr_manager, conn.seat);
        }
        static void destroy_device(Device *device) { zwlr_data_control_device_v1_destroy(device); }
        static void destroy_offer(Offer *offer) { zwlr_data_control_offer_v1_destroy(offer); }
        static Source *create_source(const Connection & conn)
        {
            return zwlr_data_control_manager_v1_create_data_source(conn.wlr_manager);
        }
        static void destroy_source(Source *source) { zwlr_data_control_source_v1_destroy(source); }
        static void offer_source(Source *source, const char *mime) { zwlr_data_control_source_v1_offer(source, mime); }
        // The data-control set_selection carries no serial.
        static void set_selection(const Connection &, Device *device, Source *source)
        {
            zwlr_data_control_device_v1_set_selection(device, source);
        }
        static void receive([[maybe_unused]] Connection & conn, Offer *offer, const char *mime, int fd)
        {
            zwlr_data_control_offer_v1_receive(offer, mime, fd);
        }

        void add_device_listener(Device *device)
        {
            zwlr_data_control_device_v1_add_listener(device, &_device_listener, this);
        }
        void add_offer_listener(Offer *offer)
        {
            zwlr_data_control_offer_v1_add_listener(offer, &_offer_listener, this);
        }
        void add_source_listener(Source *source)
        {
            zwlr_data_control_source_v1_add_listener(source, &_source_listener, this);
        }

    private:
        const zwlr_data_control_device_v1_listener _device_listener = {
            .data_offer = [](void *data,
                             Device *,
                             Offer *offer) { static_cast<WlrDataControl *>(data)->on_device_data_offer(offer); },
            .selection = [](void *data,
                            Device *,
                            Offer *offer) { static_cast<WlrDataControl *>(data)->on_device_selection(offer); },
            .finished = [](void *data, Device *) { static_cast<WlrDataControl *>(data)->on_device_finished(); },
            // Primary selection is out of scope.
            .primary_selection = [](void *, Device *, Offer *) {},
        };

        const zwlr_data_control_source_v1_listener _source_listener = {
            .send =
                [](void *data, Source *, const char *mime_type, int32_t fd) {
                    static_cast<WlrDataControl *>(data)->on_source_send(mime_type, fd);
                },
            .cancelled = [](void *data, Source *) { static_cast<WlrDataControl *>(data)->on_source_cancelled(); },
        };

        const zwlr_data_control_offer_v1_listener _offer_listener = {
            .offer =
                [](void *data, Offer *offer, const char *mime_type) {
                    static_cast<WlrDataControl *>(data)->on_offer_mime(offer, mime_type);
                },
        };
};

// core wl_data_device - fallback for compositors without data-control,
// i.e. Mutter/GNOME
class CoreDataDevice: public DataControl<CoreDataDevice>
{
    public:
        using DataControl::DataControl;

        static bool available(const Connection & conn) { return conn.data_device_manager != nullptr; }

        using Device = wl_data_device;
        using Offer = wl_data_offer;
        using Source = wl_data_source;

        static Device *get_data_device(const Connection & conn)
        {
            return wl_data_device_manager_get_data_device(conn.data_device_manager, conn.seat);
        }
        // v1 objects have no destroy request: they die with the connection.
        static void destroy_device(Device *) {}
        static void destroy_offer(Offer *) {}
        static Source *create_source(const Connection & conn)
        {
            return wl_data_device_manager_create_data_source(conn.data_device_manager);
        }
        static void destroy_source(Source *) {}
        static void offer_source(Source *source, const char *mime) { wl_data_source_offer(source, mime); }
        static void set_selection(const Connection & conn, Device *device, Source *source)
        {
            wl_data_device_set_selection(device, source, conn.keyboard_serial);
        }
        static void receive(Connection & conn, Offer *offer, const char *mime, int fd)
        {
            // Some owners require an accept before serving the selection.
            wl_data_offer_accept(offer, conn.keyboard_serial, mime);
            wl_data_offer_receive(offer, mime, fd);
        }

        void add_device_listener(Device *device) { wl_data_device_add_listener(device, &_device_listener, this); }
        void add_offer_listener(Offer *offer) { wl_data_offer_add_listener(offer, &_offer_listener, this); }
        void add_source_listener(Source *source) { wl_data_source_add_listener(source, &_source_listener, this); }

    private:
        const wl_data_device_listener _device_listener = {
            .data_offer = [](void *data,
                             Device *,
                             Offer *offer) { static_cast<CoreDataDevice *>(data)->on_device_data_offer(offer); },
            .enter = [](void *, Device *, uint32_t, wl_surface *, wl_fixed_t, wl_fixed_t, Offer *) {},
            .leave = [](void *, Device *) {},
            .motion = [](void *, Device *, uint32_t, wl_fixed_t, wl_fixed_t) {},
            .drop = [](void *, Device *) {},
            .selection = [](void *data,
                            Device *,
                            Offer *offer) { static_cast<CoreDataDevice *>(data)->on_device_selection(offer); },
        };

        const wl_data_offer_listener _offer_listener = {
            .offer =
                [](void *data, Offer *offer, const char *mime_type) {
                    static_cast<CoreDataDevice *>(data)->on_offer_mime(offer, mime_type);
                },
            .source_actions = [](void *, Offer *, uint32_t) {},
            .action = [](void *, Offer *, uint32_t) {},
        };

        const wl_data_source_listener _source_listener = {
            .target = [](void *, Source *, const char *) {},
            .send =
                [](void *data, Source *, const char *mime_type, int32_t fd) {
                    static_cast<CoreDataDevice *>(data)->on_source_send(mime_type, fd);
                },
            .cancelled = [](void *data, Source *) { static_cast<CoreDataDevice *>(data)->on_source_cancelled(); },
            .dnd_drop_performed = [](void *, Source *) {},
            .dnd_finished = [](void *, Source *) {},
            .action = [](void *, Source *, uint32_t) {},
        };
};

// Read and write flows, tried on each protocol in order

// Reads the wanted mime types the current selection offers through the first
// available protocol; a protocol hard-failing hands the read to the next one.
std::vector<RawContent> read_contents(const std::vector<std::string_view> & wanted_mimes)
{
    // One budget for the whole read: connection roundtrips, offer discovery
    // and every mime fetch.
    const Timestamp deadline = internal::deadline_from_now(get_timeout);
    Connection conn(deadline);
    if (!conn.ok())
    {
        SIHD_LOG(error, "wayland: could not connect to the display");
        return {};
    }

    bool attempted = false;
    if (ExtDataControl::available(conn))
    {
        attempted = true;
        // A fresh state per attempt: offers left by another protocol would end it early.
        SelectionState state;
        if (auto contents = ExtDataControl(conn, state).read(wanted_mimes, deadline); contents.has_value())
            return *contents;
    }
    if (WlrDataControl::available(conn))
    {
        attempted = true;
        SelectionState state;
        if (auto contents = WlrDataControl(conn, state).read(wanted_mimes, deadline); contents.has_value())
            return *contents;
    }
    if (CoreDataDevice::available(conn))
    {
        attempted = true;
        SelectionState state;
        if (auto contents = CoreDataDevice(conn, state).read(wanted_mimes, deadline); contents.has_value())
            return *contents;
    }

    if (attempted)
        SIHD_LOG(error, "wayland: every available clipboard protocol failed to read");
    else
        SIHD_LOG(error, "wayland: compositor supports no clipboard protocol");
    return {};
}

// Offers the representations through the first available protocol, serving
// within the deadline set by the platform dispatcher.
bool write_all(const std::vector<RawContentView> & contents, Timestamp deadline)
{
    if (contents.empty())
        return false;

    Connection conn(deadline);
    if (!conn.ok())
    {
        SIHD_LOG(error, "wayland: could not connect to the display");
        return false;
    }

    SelectionState state;
    state.deadline = deadline;
    bool served = false;
    if (ExtDataControl::available(conn))
        served = ExtDataControl(conn, state).write(contents, deadline);
    else if (WlrDataControl::available(conn))
        served = WlrDataControl(conn, state).write(contents, deadline);
    else if (CoreDataDevice::available(conn))
        served = CoreDataDevice(conn, state).write(contents, deadline);
    else
    {
        SIHD_LOG(error, "wayland: compositor supports no clipboard protocol");
        return false;
    }

    if (!served && !state.cancelled)
    {
        SIHD_LOG(warning, "wayland: no application requested the clipboard content in time, it will be lost");
    }
    return served;
}

} // namespace

std::vector<RawContent> get_raw()
{
    return get_raw({});
}

std::vector<RawContent> get_raw(const std::vector<std::string_view> & wanted_mimes)
{
    return read_contents(wanted_mimes);
}

bool set_raw(const std::vector<RawContentView> & contents, Timestamp deadline)
{
    return write_all(contents, deadline);
}

} // namespace sihd::sys::clipboard::wayland
