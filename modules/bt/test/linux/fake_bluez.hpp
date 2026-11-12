#ifndef __SIHD_BT_TEST_FAKE_BLUEZ_HPP__
#define __SIHD_BT_TEST_FAKE_BLUEZ_HPP__

#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <fake_dbus_daemon.hpp>
#include <map>
#include <string>
#include <vector>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Result.hpp>

namespace test
{

inline constexpr char bluez_service[] = "org.bluez";
inline constexpr char adapter_interface[] = "org.bluez.Adapter1";
inline constexpr char device_interface[] = "org.bluez.Device1";
inline constexpr char agent_manager_interface[] = "org.bluez.AgentManager1";
inline constexpr char agent_interface[] = "org.bluez.Agent1";
inline constexpr char profile_manager_interface[] = "org.bluez.ProfileManager1";
inline constexpr char profile_interface[] = "org.bluez.Profile1";
inline constexpr char gatt_manager_interface[] = "org.bluez.GattManager1";
inline constexpr char gatt_service_interface[] = "org.bluez.GattService1";
inline constexpr char gatt_characteristic_interface[] = "org.bluez.GattCharacteristic1";
inline constexpr char gatt_descriptor_interface[] = "org.bluez.GattDescriptor1";
inline constexpr char advertising_manager_interface[] = "org.bluez.LEAdvertisingManager1";
inline constexpr char advertisement_interface[] = "org.bluez.LEAdvertisement1";
inline constexpr char properties_interface[] = "org.freedesktop.DBus.Properties";
inline constexpr char object_manager_interface[] = "org.freedesktop.DBus.ObjectManager";

inline constexpr char manager_path[] = "/org/bluez";
inline constexpr char adapter_path[] = "/org/bluez/hci0";
inline constexpr char second_adapter_path[] = "/org/bluez/hci1";
inline constexpr char device_address[] = "AA:BB:CC:DD:EE:FF";
inline constexpr char device_path[] = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF";

// battery service 0x000c: level 0x000d read|notify with its CCCD 0x000f, alert 0x0010 indicate
inline constexpr char service_path[] = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/service000c";
inline constexpr char level_path[] = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/service000c/char000d";
inline constexpr char cccd_path[] = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/service000c/char000d/desc000f";
inline constexpr char alert_path[] = "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/service000c/char0010";
inline constexpr char battery_uuid[] = "0000180f-0000-1000-8000-00805f9b34fb";
inline constexpr char level_uuid[] = "00002a19-0000-1000-8000-00805f9b34fb";
inline constexpr char cccd_uuid[] = "00002902-0000-1000-8000-00805f9b34fb";
inline constexpr char alert_uuid[] = "00002a06-0000-1000-8000-00805f9b34fb";
inline constexpr char serial_port_uuid[] = "00001101-0000-1000-8000-00805f9b34fb";

inline FakeValue i16(int16_t value)
{
    return {"n", [value](dbus::Appender & to) { return to.add(value); }};
}

inline FakeValue u16(uint16_t value)
{
    return {"q", [value](dbus::Appender & to) { return to.add(value); }};
}

// libdbus sends a duplicate: the caller keeps its fd
inline FakeValue unix_fd(int fd)
{
    return {"h", [fd](dbus::Appender & to) { return to.add_unix_fd(fd); }};
}

// the a{sa{sv}} of InterfacesAdded: one interface and its properties
inline FakeValue interfaces(const std::string & interface, const FakeEntries & properties)
{
    return {"a{sa{sv}}", [interface, properties](dbus::Appender & to) -> sihd::util::Result<void> {
                auto array = to.open_array("{sa{sv}}");
                SIHD_UNEXPECTED_RETURN(array);
                auto entry = array->open_dict_entry();
                SIHD_UNEXPECTED_RETURN(entry);
                auto name_added = entry->add(interface);
                SIHD_UNEXPECTED_RETURN(name_added);
                auto appended = append_entries(*entry, properties);
                SIHD_UNEXPECTED_RETURN(appended);
                auto entry_closed = array->close(std::move(*entry));
                SIHD_UNEXPECTED_RETURN(entry_closed);
                return to.close(std::move(*array));
            }};
}

inline void answer(FakeDaemon & daemon, const std::string & interface, const std::vector<std::string> & members)
{
    for (const std::string & member : members)
        daemon.on(interface, member, [&daemon](const dbus::Message & call) { daemon.reply(call, {}); });
}

inline void serve_adapter(FakeDaemon & daemon, const std::string & path, bool powered)
{
    daemon.set_property(path, adapter_interface, "Address", text("00:11:22:33:44:55"));
    daemon.set_property(path, adapter_interface, "Name", text("fake"));
    daemon.set_property(path, adapter_interface, "Alias", text("fake alias"));
    daemon.set_property(path, adapter_interface, "Powered", boolean(powered));
    daemon.set_property(path, adapter_interface, "Discoverable", boolean(false));
    daemon.set_property(path, adapter_interface, "Pairable", boolean(true));
    daemon.set_property(path, adapter_interface, "Discovering", boolean(false));
}

// a device of hci0 that advertised during the current discovery
inline FakeEntries device_properties(const std::string & address, const std::string & name, int16_t rssi)
{
    return {
        {"Adapter", object_path(adapter_path)},
        {"Address", text(address)},
        {"AddressType", text("random")},
        {"Name", text(name)},
        {"Paired", boolean(false)},
        {"Connected", boolean(false)},
        {"Trusted", boolean(false)},
        {"ServicesResolved", boolean(false)},
        {"RSSI", i16(rssi)},
        {"UUIDs", texts({battery_uuid})},
        {"Appearance", u16(0x0300)},
    };
}

inline void serve_device(FakeDaemon & daemon)
{
    for (const auto & [name, value] : device_properties(device_address, "thermometer", -60))
        daemon.set_property(device_path, device_interface, name, value);
}

inline void serve_gatt(FakeDaemon & daemon)
{
    daemon.set_property(service_path, gatt_service_interface, "UUID", text(battery_uuid));
    daemon.set_property(service_path, gatt_service_interface, "Primary", boolean(true));
    daemon.set_property(service_path, gatt_service_interface, "Device", object_path(device_path));
    daemon.set_property(service_path, gatt_service_interface, "Handle", u16(0x000c));

    daemon.set_property(level_path, gatt_characteristic_interface, "UUID", text(level_uuid));
    daemon.set_property(level_path, gatt_characteristic_interface, "Service", object_path(service_path));
    daemon.set_property(level_path, gatt_characteristic_interface, "Flags", texts({"read", "notify"}));
    daemon.set_property(level_path, gatt_characteristic_interface, "Handle", u16(0x000d));
    daemon.set_property(level_path, gatt_characteristic_interface, "MTU", u16(247));

    daemon.set_property(cccd_path, gatt_descriptor_interface, "UUID", text(cccd_uuid));
    daemon.set_property(cccd_path, gatt_descriptor_interface, "Characteristic", object_path(level_path));
    daemon.set_property(cccd_path, gatt_descriptor_interface, "Handle", u16(0x000f));

    daemon.set_property(alert_path, gatt_characteristic_interface, "UUID", text(alert_uuid));
    daemon.set_property(alert_path, gatt_characteristic_interface, "Service", object_path(service_path));
    daemon.set_property(alert_path, gatt_characteristic_interface, "Flags", texts({"indicate"}));
    daemon.set_property(alert_path, gatt_characteristic_interface, "Handle", u16(0x0010));
}

// both ends of a unix socket pair, closed with it
class SocketPair
{
    public:
        explicit SocketPair(int type)
        {
            if (::socketpair(AF_UNIX, type | SOCK_CLOEXEC, 0, _fds.data()) < 0)
                _fds = {-1, -1};
        }

        ~SocketPair()
        {
            for (const int fd : _fds)
            {
                if (fd >= 0)
                    ::close(fd);
            }
        }

        SocketPair(const SocketPair &) = delete;
        SocketPair & operator=(const SocketPair &) = delete;

        int local() const { return _fds[0]; }
        int remote() const { return _fds[1]; }

        std::string read_remote() const
        {
            std::array<char, 256> buffer {};
            const ssize_t received = ::recv(_fds[1], buffer.data(), buffer.size(), 0);
            return received > 0 ? std::string(buffer.data(), static_cast<size_t>(received)) : std::string();
        }

        bool write_remote(const std::string & data) const
        {
            return ::send(_fds[1], data.data(), data.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(data.size());
        }

    private:
        std::array<int, 2> _fds;
};

// the a{sv} properties of one interface, each value read as a string, a bool or a string array
struct ReadProperties
{
        std::map<std::string, std::string> texts;
        std::map<std::string, bool> booleans;
        std::map<std::string, std::vector<std::string>> arrays;
};

inline sihd::util::Result<void> read_properties(dbus::Iter & properties, ReadProperties & into)
{
    return properties.for_each_entry<std::string>(
        [&into](const std::string & key, dbus::Iter & value) -> sihd::util::Result<void> {
            if (auto text = value.get<std::string>())
                into.texts.insert_or_assign(key, *text);
            else if (auto flag = value.get<bool>())
                into.booleans.insert_or_assign(key, *flag);
            else if (auto array = value.get_array<std::string>())
                into.arrays.insert_or_assign(key, *array);
            return {};
        });
}

} // namespace test

#endif
