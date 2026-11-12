#include <string>

#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <optional>
# include <utility>

# include <sihd/sys/linux/dbus.hpp>

# include "internal/desktop_bus.hpp"
# include "internal/login1.hpp"
#endif

#include <sihd/sys/PowerInhibitor.hpp>

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

#if defined(__SIHD_LINUX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

namespace dbus = sihd::sys::linux_::dbus;

namespace
{

constexpr uint32_t portal_inhibit_idle = 8;

Result<dbus::Message> portal_inhibit_request(ZStringView reason)
{
    dbus::Message request;
    auto built = request.method_call(internal::portal_service,
                                     internal::portal_path,
                                     "org.freedesktop.portal.Inhibit",
                                     "Inhibit");
    SIHD_UNEXPECTED_RETURN(built);
    dbus::Appender body(request);
    auto added = body.add(ZStringView(""));
    SIHD_UNEXPECTED_RETURN(added);
    added = body.add(portal_inhibit_idle);
    SIHD_UNEXPECTED_RETURN(added);

    auto options = body.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(options);
    added = options->add_entry("reason", "s", [reason](dbus::Appender & value) { return value.add(reason); });
    SIHD_UNEXPECTED_RETURN(added);
    added = body.close(std::move(*options));
    SIHD_UNEXPECTED_RETURN(added);
    return request;
}

} // namespace

struct PowerInhibitor::Impl
{
        ~Impl() { (void)this->release_display(); }

        Result<void> release_display()
        {
            if (display_handle.empty())
                return {};
            const std::string handle = std::exchange(display_handle, "");
            dbus::Message request;
            auto built = request.method_call(internal::portal_service,
                                             handle,
                                             "org.freedesktop.portal.Request",
                                             "Close");
            SIHD_UNEXPECTED_RETURN(built);
            auto reply = request.call(portal, internal::dbus_call_timeout);
            portal.close();
            SIHD_UNEXPECTED_RETURN(reply);
            return {};
        }

        std::string app_name;
        std::optional<dbus::UnixFd> sleep_lock;
        // the portal drops the inhibition of a lost connection: it stays open as long as the inhibition
        dbus::Connection portal;
        std::string display_handle;
};

Result<void> PowerInhibitor::inhibit_sleep(ZStringView reason)
{
    if (_impl->sleep_lock.has_value())
        return std::unexpected(Error(already_exists, "sleep is already inhibited"));
    dbus::Connection connection;
    auto opened = connection.open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    auto lock = internal::login1_inhibit_sleep(connection, _impl->app_name, reason, "block");
    SIHD_UNEXPECTED_RETURN(lock);
    _impl->sleep_lock = std::move(*lock);
    return {};
}

Result<void> PowerInhibitor::inhibit_display(ZStringView reason)
{
    if (!_impl->display_handle.empty())
        return std::unexpected(Error(already_exists, "display is already inhibited"));
    auto request = portal_inhibit_request(reason);
    SIHD_UNEXPECTED_RETURN(request);
    auto opened = _impl->portal.open(dbus::Bus::session);
    SIHD_UNEXPECTED_RETURN(opened);
    auto reply = request->call(_impl->portal, internal::dbus_call_timeout);
    if (!reply.has_value())
        _impl->portal.close();
    SIHD_UNEXPECTED_RETURN(reply);
    auto handle = dbus::Iter(*reply).get<std::string>();
    if (!handle.has_value())
        _impl->portal.close();
    SIHD_UNEXPECTED_RETURN(handle);
    _impl->display_handle = std::move(*handle);
    return {};
}

Result<void> PowerInhibitor::release()
{
    _impl->sleep_lock.reset();
    return _impl->release_display();
}

bool PowerInhibitor::is_sleep_inhibited() const
{
    return _impl->sleep_lock.has_value();
}

bool PowerInhibitor::is_display_inhibited() const
{
    return !_impl->display_handle.empty();
}

#else

struct PowerInhibitor::Impl
{
        std::string app_name;
};

Result<void> PowerInhibitor::inhibit_sleep(ZStringView)
{
    return std::unexpected(Error(not_supported, "power inhibition is not supported on this platform"));
}

Result<void> PowerInhibitor::inhibit_display(ZStringView)
{
    return std::unexpected(Error(not_supported, "power inhibition is not supported on this platform"));
}

Result<void> PowerInhibitor::release()
{
    return {};
}

bool PowerInhibitor::is_sleep_inhibited() const
{
    return false;
}

bool PowerInhibitor::is_display_inhibited() const
{
    return false;
}

#endif

PowerInhibitor::PowerInhibitor(): _impl(std::make_unique<Impl>()) {}

PowerInhibitor::PowerInhibitor(PowerInhibitor && other) = default;

PowerInhibitor & PowerInhibitor::operator=(PowerInhibitor && other) = default;

PowerInhibitor::~PowerInhibitor() = default;

void PowerInhibitor::set_app_name(const std::string & name)
{
    _impl->app_name = name;
}

} // namespace sihd::sys
