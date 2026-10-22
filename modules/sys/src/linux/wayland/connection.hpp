#ifndef __SIHD_SYS_WAYLAND_CONNECTION_HPP__
#define __SIHD_SYS_WAYLAND_CONNECTION_HPP__

// Shared wayland plumbing: bounded event loops and an RAII connection; a wedged compositor must not hang a call.

#include <functional>
#include <wayland-client.h>

#include <sihd/util/Timestamp.hpp>

namespace sihd::sys::internal::wayland
{

class Connection
{
    public:
        Connection();

        virtual ~Connection();

        Connection(const Connection &) = delete;
        Connection & operator=(const Connection &) = delete;

        bool ok() const { return _ok; }

        wl_display *display() { return _display; }

        int fd() const { return _fd; }

    protected:
        bool bind_globals(sihd::util::Timestamp deadline);

        virtual void bind_global(wl_registry *registry, uint32_t name, const char *interface, uint32_t version) = 0;

        virtual bool ready() const = 0;

    private:
        const wl_registry_listener _registry_listener = {
            .global =
                [](void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
                    static_cast<Connection *>(data)->bind_global(registry, name, interface, version);
                },
            .global_remove = [](void *, wl_registry *, uint32_t) {},
        };

        wl_display *_display = nullptr;
        wl_registry *_registry = nullptr;
        int _fd = -1;
        bool _ok = false;
};

bool dispatch_until(Connection & conn, sihd::util::Timestamp deadline, const std::function<bool()> & stop);

bool roundtrip_bounded(Connection & conn, sihd::util::Timestamp deadline);

} // namespace sihd::sys::internal::wayland

#endif
