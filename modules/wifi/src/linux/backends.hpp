#ifndef __SIHD_WIFI_LINUX_BACKENDS_HPP__
#define __SIHD_WIFI_LINUX_BACKENDS_HPP__

#include <optional>
#include <string>
#include <vector>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/time.hpp>
#include <sihd/wifi/Monitor.hpp>
#include <sihd/wifi/connection.hpp>

namespace sihd::wifi
{

const sihd::util::Duration call_timeout(sihd::util::time::sec(10));

} // namespace sihd::wifi

namespace sihd::wifi::network_manager
{

namespace dbus = sihd::sys::linux_::dbus;

// nullopt unless networkmanager manages that wifi interface, its device object path otherwise
sihd::util::Result<std::optional<std::string>> detect(dbus::Connection & connection,
                                                      const std::string & interface_name);
sihd::util::Result<State> state(dbus::Connection & connection, const std::string & device);
sihd::util::Result<void> scan(dbus::Connection & connection, const std::string & device, sihd::util::Duration timeout);
sihd::util::Result<void> connect(dbus::Connection & connection,
                                 const std::string & device,
                                 const std::string & ssid,
                                 const std::optional<std::string> & passphrase,
                                 sihd::util::Duration timeout);
sihd::util::Result<void> disconnect(dbus::Connection & connection, const std::string & device);
sihd::util::Result<std::vector<KnownNetwork>> known_networks(dbus::Connection & connection, const std::string & device);
sihd::util::Result<void> forget(dbus::Connection & connection, const std::string & device, const std::string & ssid);

std::vector<std::string> monitor_rules(const std::string & device);
sihd::util::Result<std::vector<Monitor::Event>> events_of(const dbus::Message & message, const std::string & device);

} // namespace sihd::wifi::network_manager

namespace sihd::wifi::iwd
{

namespace dbus = sihd::sys::linux_::dbus;

// nullopt unless iwd manages that wifi interface, its station object path otherwise
sihd::util::Result<std::optional<std::string>> detect(dbus::Connection & connection,
                                                      const std::string & interface_name);
sihd::util::Result<State> state(dbus::Connection & connection, const std::string & station);
sihd::util::Result<void> scan(dbus::Connection & connection, const std::string & station, sihd::util::Duration timeout);
sihd::util::Result<void> connect(dbus::Connection & connection,
                                 const std::string & station,
                                 const std::string & ssid,
                                 const std::optional<std::string> & passphrase,
                                 sihd::util::Duration timeout);
sihd::util::Result<void> disconnect(dbus::Connection & connection, const std::string & station);
sihd::util::Result<std::vector<KnownNetwork>> known_networks(dbus::Connection & connection,
                                                             const std::string & station);
sihd::util::Result<void> forget(dbus::Connection & connection, const std::string & station, const std::string & ssid);

std::vector<std::string> monitor_rules(const std::string & station);
sihd::util::Result<std::vector<Monitor::Event>> events_of(const dbus::Message & message, const std::string & station);

} // namespace sihd::wifi::iwd

namespace sihd::wifi::wpa_supplicant
{

namespace dbus = sihd::sys::linux_::dbus;

// nullopt unless wpa_supplicant controls that wifi interface, its interface object path otherwise
sihd::util::Result<std::optional<std::string>> detect(dbus::Connection & connection,
                                                      const std::string & interface_name);
sihd::util::Result<State> state(dbus::Connection & connection, const std::string & interface);
sihd::util::Result<void>
    scan(dbus::Connection & connection, const std::string & interface, sihd::util::Duration timeout);
sihd::util::Result<void> connect(dbus::Connection & connection,
                                 const std::string & interface,
                                 const std::string & ssid,
                                 const std::optional<std::string> & passphrase,
                                 sihd::util::Duration timeout);
sihd::util::Result<void> disconnect(dbus::Connection & connection, const std::string & interface);
sihd::util::Result<std::vector<KnownNetwork>> known_networks(dbus::Connection & connection,
                                                             const std::string & interface);
sihd::util::Result<void> forget(dbus::Connection & connection, const std::string & interface, const std::string & ssid);

std::vector<std::string> monitor_rules(const std::string & interface);
sihd::util::Result<std::vector<Monitor::Event>> events_of(const dbus::Message & message, const std::string & interface);

} // namespace sihd::wifi::wpa_supplicant

namespace sihd::wifi
{

struct Daemon
{
        Backend backend = Backend::network_manager;
        std::string path;
};

// networkmanager must come first: it drives iwd or wpa_supplicant when it runs over them
sihd::util::Result<Daemon> detect(sihd::sys::linux_::dbus::Connection & connection, const std::string & interface_name);

} // namespace sihd::wifi

#endif
