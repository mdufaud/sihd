#ifndef __SIHD_WIFI_TEST_FAKE_DAEMON_HPP__
#define __SIHD_WIFI_TEST_FAKE_DAEMON_HPP__

#include <fake_dbus_daemon.hpp>
#include <string>
#include <utility>
#include <vector>

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

inline FakeValue profile_settings(const std::vector<std::pair<std::string, FakeEntries>> & groups)
{
    return {"a{sa{sv}}", [groups](dbus::Appender & to) -> sihd::util::Result<void> {
                auto array = to.open_array("{sa{sv}}");
                SIHD_UNEXPECTED_RETURN(array);
                for (const auto & [name, entries] : groups)
                {
                    auto entry = array->open_dict_entry();
                    SIHD_UNEXPECTED_RETURN(entry);
                    auto name_added = entry->add(name);
                    SIHD_UNEXPECTED_RETURN(name_added);
                    auto appended = append_entries(*entry, entries);
                    SIHD_UNEXPECTED_RETURN(appended);
                    auto entry_closed = array->close(std::move(*entry));
                    SIHD_UNEXPECTED_RETURN(entry_closed);
                }
                return to.close(std::move(*array));
            }};
}

// networkmanager managing wlan-test, a wifi device unless told otherwise
inline void serve_network_manager(FakeDaemon & daemon, bool managed)
{
    daemon.set_property(nm_device, nm_device_interface, "Managed", boolean(managed));
    daemon.set_property(nm_device, nm_device_interface, "DeviceType", u32(2));
    daemon.on(nm_service, "GetDeviceByIpIface", [&daemon](const dbus::Message & call) {
        if (dbus::Iter(call).get<std::string>().value_or("") != fake_interface_name)
            return daemon.fail(call, "org.freedesktop.NetworkManager.UnknownDevice", "No such device");
        daemon.reply(call, {object_path(nm_device)});
    });
}

inline void serve_iwd_station(FakeDaemon & daemon, const std::string & state)
{
    daemon.set_property(iwd_station, iwd_device_interface, "Name", text(fake_interface_name));
    daemon.set_property(iwd_station, iwd_station_interface, "State", text(state));
}

inline void serve_wpa_supplicant(FakeDaemon & daemon)
{
    daemon.on(wpa_service, "GetInterface", [&daemon](const dbus::Message & call) {
        if (dbus::Iter(call).get<std::string>().value_or("") != fake_interface_name)
            return daemon.fail(call, "fi.w1.wpa_supplicant1.InterfaceUnknown", "wpa_supplicant knows nothing");
        daemon.reply(call, {object_path(wpa_interface)});
    });
}

} // namespace test

#endif
