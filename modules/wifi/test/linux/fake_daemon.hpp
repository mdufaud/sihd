#ifndef __SIHD_WIFI_TEST_FAKE_DAEMON_HPP__
#define __SIHD_WIFI_TEST_FAKE_DAEMON_HPP__

#include <fake_dbus_daemon.hpp>
#include <map>
#include <string>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Result.hpp>

namespace test
{

inline constexpr char fake_interface_name[] = "wlan-test";

inline constexpr char nm_service[] = "org.freedesktop.NetworkManager";
inline constexpr char nm_device[] = "/org/freedesktop/NetworkManager/Devices/3";
inline constexpr char nm_device_interface[] = "org.freedesktop.NetworkManager.Device";
inline constexpr char nm_wireless_interface[] = "org.freedesktop.NetworkManager.Device.Wireless";

inline constexpr char iwd_service[] = "net.connman.iwd";
inline constexpr char iwd_station[] = "/net/connman/iwd/0/4";
inline constexpr char iwd_device_interface[] = "net.connman.iwd.Device";
inline constexpr char iwd_station_interface[] = "net.connman.iwd.Station";

inline constexpr char wpa_service[] = "fi.w1.wpa_supplicant1";
inline constexpr char wpa_interface[] = "/fi/w1/wpa_supplicant1/Interfaces/0";
inline constexpr char wpa_interface_interface[] = "fi.w1.wpa_supplicant1.Interface";

// the a{sa{sv}} of a networkmanager profile: settings by group
using ProfileSettings = std::map<std::string, dbus::Dict>;

// networkmanager managing wlan-test, a wifi device unless told otherwise
inline void serve_network_manager(FakeDaemon & daemon, bool managed)
{
    daemon.set_property(nm_device, nm_device_interface, "Managed", managed);
    daemon.set_property(nm_device, nm_device_interface, "DeviceType", uint32_t {2});
    daemon.on(nm_service, "GetDeviceByIpIface", [&daemon](const dbus::Message & call) {
        if (dbus::Iter(call).get<std::string>().value_or("") != fake_interface_name)
            return daemon.fail(call, "org.freedesktop.NetworkManager.UnknownDevice", "No such device");
        daemon.reply(call, {dbus::ObjectPath(nm_device)});
    });
}

inline void serve_iwd_station(FakeDaemon & daemon, const std::string & state)
{
    daemon.set_property(iwd_station, iwd_device_interface, "Name", fake_interface_name);
    daemon.set_property(iwd_station, iwd_station_interface, "State", state);
}

inline void serve_wpa_supplicant(FakeDaemon & daemon)
{
    daemon.on(wpa_service, "GetInterface", [&daemon](const dbus::Message & call) {
        if (dbus::Iter(call).get<std::string>().value_or("") != fake_interface_name)
            return daemon.fail(call, "fi.w1.wpa_supplicant1.InterfaceUnknown", "wpa_supplicant knows nothing");
        daemon.reply(call, {dbus::ObjectPath(wpa_interface)});
    });
}

} // namespace test

#endif
