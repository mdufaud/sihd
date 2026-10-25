#include <sihd/sys/linux/dbus.hpp>
#include <sihd/wifi/Monitor.hpp>

#include "backends.hpp"

using namespace sihd::util;
using namespace sihd::sys::linux_;

namespace sihd::wifi
{

namespace
{

std::vector<std::string> monitor_rules(const Daemon & daemon)
{
    switch (daemon.backend)
    {
        case Backend::network_manager:
            return network_manager::monitor_rules(daemon.path);
        case Backend::iwd:
            return iwd::monitor_rules(daemon.path);
        case Backend::wpa_supplicant:
        default:
            return wpa_supplicant::monitor_rules(daemon.path);
    }
}

Result<std::vector<Monitor::Event>> events_of(const Daemon & daemon, const dbus::Message & message)
{
    switch (daemon.backend)
    {
        case Backend::network_manager:
            return network_manager::events_of(message, daemon.path);
        case Backend::iwd:
            return iwd::events_of(message, daemon.path);
        case Backend::wpa_supplicant:
        default:
            return wpa_supplicant::events_of(message, daemon.path);
    }
}

} // namespace

struct Monitor::Impl
{
        dbus::Connection connection;
        Daemon daemon;
};

Monitor::Monitor(): _impl(std::make_unique<Impl>()) {}

Monitor::~Monitor() = default;

Result<void> Monitor::open(const std::string & interface_name)
{
    if (_impl->connection.is_open())
        return std::unexpected(Error(ErrorCode::already_exists, "the monitor is already open"));
    dbus::Connection connection;
    connection.disable_auto_start();
    auto opened = connection.open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    auto detected = detect(connection, interface_name);
    SIHD_UNEXPECTED_RETURN(detected);
    for (const std::string & rule : monitor_rules(*detected))
    {
        auto matched = connection.add_match(rule);
        SIHD_UNEXPECTED_RETURN(matched);
    }
    _impl->connection = std::move(connection);
    _impl->daemon = std::move(*detected);
    return {};
}

Result<std::optional<std::vector<Monitor::Event>>> Monitor::_receive(Duration timeout)
{
    auto received = dbus::Message::receive(_impl->connection, timeout);
    SIHD_UNEXPECTED_RETURN(received);
    if (!received->has_value())
        return std::nullopt;
    return events_of(_impl->daemon, **received);
}

} // namespace sihd::wifi
