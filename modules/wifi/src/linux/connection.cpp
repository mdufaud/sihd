#include <optional>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/scan.hpp>

#include "../connect.hpp"
#include "backends.hpp"

using namespace sihd::util;
using namespace sihd::sys::linux_;

namespace sihd::wifi
{

namespace
{

struct Session
{
        dbus::Connection connection;
        Daemon daemon;
};

Result<Session> open(const std::string & interface_name)
{
    Session session;
    session.connection.disable_auto_start();
    auto opened = session.connection.open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    auto detected = detect(session.connection, interface_name);
    SIHD_UNEXPECTED_RETURN(detected);
    session.daemon = std::move(*detected);
    return session;
}

} // namespace

Result<Daemon> detect(dbus::Connection & connection, const std::string & interface_name)
{
    auto network_manager_device = network_manager::detect(connection, interface_name);
    SIHD_UNEXPECTED_RETURN(network_manager_device);
    if (network_manager_device->has_value())
        return Daemon {.backend = Backend::network_manager, .path = **network_manager_device};
    auto iwd_station = iwd::detect(connection, interface_name);
    SIHD_UNEXPECTED_RETURN(iwd_station);
    if (iwd_station->has_value())
        return Daemon {.backend = Backend::iwd, .path = **iwd_station};
    auto supplicant_interface = wpa_supplicant::detect(connection, interface_name);
    SIHD_UNEXPECTED_RETURN(supplicant_interface);
    if (supplicant_interface->has_value())
        return Daemon {.backend = Backend::wpa_supplicant, .path = **supplicant_interface};
    return std::unexpected(Error(ErrorCode::not_found, "no wifi daemon manages {}", interface_name));
}

Result<Backend> backend(const std::string & interface_name)
{
    auto session = open(interface_name);
    SIHD_UNEXPECTED_RETURN(session);
    return session->daemon.backend;
}

Result<State> state(const std::string & interface_name)
{
    auto session = open(interface_name);
    SIHD_UNEXPECTED_RETURN(session);
    switch (session->daemon.backend)
    {
        case Backend::network_manager:
            return network_manager::state(session->connection, session->daemon.path);
        case Backend::iwd:
            return iwd::state(session->connection, session->daemon.path);
        case Backend::wpa_supplicant:
        default:
            return wpa_supplicant::state(session->connection, session->daemon.path);
    }
}

Result<void> scan(const std::string & interface_name, Duration timeout)
{
    auto session = open(interface_name);
    SIHD_UNEXPECTED_RETURN(session);
    switch (session->daemon.backend)
    {
        case Backend::network_manager:
            return network_manager::scan(session->connection, session->daemon.path, timeout);
        case Backend::iwd:
            return iwd::scan(session->connection, session->daemon.path, timeout);
        case Backend::wpa_supplicant:
        default:
            return wpa_supplicant::scan(session->connection, session->daemon.path, timeout);
    }
}

Result<void> connect_backend(const std::string & interface_name,
                             const std::string & ssid,
                             const std::optional<std::string> & passphrase,
                             Duration timeout)
{
    auto session = open(interface_name);
    SIHD_UNEXPECTED_RETURN(session);
    switch (session->daemon.backend)
    {
        case Backend::network_manager:
            return network_manager::connect(session->connection, session->daemon.path, ssid, passphrase, timeout);
        case Backend::iwd:
            return iwd::connect(session->connection, session->daemon.path, ssid, passphrase, timeout);
        case Backend::wpa_supplicant:
        default:
            return wpa_supplicant::connect(session->connection, session->daemon.path, ssid, passphrase, timeout);
    }
}

Result<void> disconnect(const std::string & interface_name)
{
    auto session = open(interface_name);
    SIHD_UNEXPECTED_RETURN(session);
    switch (session->daemon.backend)
    {
        case Backend::network_manager:
            return network_manager::disconnect(session->connection, session->daemon.path);
        case Backend::iwd:
            return iwd::disconnect(session->connection, session->daemon.path);
        case Backend::wpa_supplicant:
        default:
            return wpa_supplicant::disconnect(session->connection, session->daemon.path);
    }
}

Result<std::vector<KnownNetwork>> known_networks(const std::string & interface_name)
{
    auto session = open(interface_name);
    SIHD_UNEXPECTED_RETURN(session);
    switch (session->daemon.backend)
    {
        case Backend::network_manager:
            return network_manager::known_networks(session->connection, session->daemon.path);
        case Backend::iwd:
            return iwd::known_networks(session->connection, session->daemon.path);
        case Backend::wpa_supplicant:
        default:
            return wpa_supplicant::known_networks(session->connection, session->daemon.path);
    }
}

Result<void> forget(const std::string & interface_name, const std::string & ssid)
{
    auto session = open(interface_name);
    SIHD_UNEXPECTED_RETURN(session);
    switch (session->daemon.backend)
    {
        case Backend::network_manager:
            return network_manager::forget(session->connection, session->daemon.path, ssid);
        case Backend::iwd:
            return iwd::forget(session->connection, session->daemon.path, ssid);
        case Backend::wpa_supplicant:
        default:
            return wpa_supplicant::forget(session->connection, session->daemon.path, ssid);
    }
}

} // namespace sihd::wifi
