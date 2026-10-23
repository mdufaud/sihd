#include "connection.hpp"

#include <sihd/sys/Poll.hpp>

#include "../internal/deadline.hpp"
#include "../internal/desktop_env.hpp"

namespace sihd::sys::internal
{

bool wayland_compiled()
{
    return true;
}

} // namespace sihd::sys::internal

namespace sihd::sys::internal::wayland
{

using sihd::util::Timestamp;

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

        const int ret = poll.poll(poll_timeout_ms(deadline));
        if (ret <= 0)
        {
            wl_display_cancel_read(display);
            if (poll.polling_error() || steady_now() >= deadline)
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

bool roundtrip_bounded(Connection & conn, Timestamp deadline)
{
    bool done = false;
    const wl_callback_listener listener = {
        .done = [](void *data, wl_callback *, uint32_t) { *static_cast<bool *>(data) = true; },
    };
    wl_callback *callback = wl_display_sync(conn.display());
    if (callback == nullptr)
        return false;
    wl_callback_add_listener(callback, &listener, &done);
    const bool ok = dispatch_until(conn, deadline, [&] { return done; });
    wl_callback_destroy(callback);
    return ok;
}

Connection::Connection()
{
    _display = wl_display_connect(nullptr);
    if (_display == nullptr)
        return;
    _fd = wl_display_get_fd(_display);
    _registry = wl_display_get_registry(_display);
    wl_registry_add_listener(_registry, &_registry_listener, this);
}

bool Connection::bind_globals(Timestamp deadline)
{
    if (_registry == nullptr)
        return false;
    // First roundtrip binds the globals, the second lets them emit initial events (seat caps, output names)
    if (!roundtrip_bounded(*this, deadline) || !roundtrip_bounded(*this, deadline))
        return false;
    _ok = ready();
    return _ok;
}

Connection::~Connection()
{
    if (_registry != nullptr)
        wl_registry_destroy(_registry);
    if (_display != nullptr)
        wl_display_disconnect(_display);
}

} // namespace sihd::sys::internal::wayland
