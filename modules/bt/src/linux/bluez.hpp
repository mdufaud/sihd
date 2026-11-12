#ifndef __SIHD_BT_LINUX_BLUEZ_HPP__
#define __SIHD_BT_LINUX_BLUEZ_HPP__

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <sihd/bt/Device.hpp>
#include <sihd/bt/Scanner.hpp>
#include <sihd/bt/adapter.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::bt::bluez
{

namespace dbus = sihd::sys::linux_::dbus;

inline constexpr sihd::util::ZStringView service = "org.bluez";
inline constexpr sihd::util::ZStringView manager_path = "/org/bluez";
inline constexpr sihd::util::ZStringView adapter_interface = "org.bluez.Adapter1";
inline constexpr sihd::util::ZStringView device_interface = "org.bluez.Device1";
inline constexpr sihd::util::ZStringView agent_manager_interface = "org.bluez.AgentManager1";
inline constexpr sihd::util::ZStringView agent_interface = "org.bluez.Agent1";
inline constexpr sihd::util::ZStringView profile_manager_interface = "org.bluez.ProfileManager1";
inline constexpr sihd::util::ZStringView profile_interface = "org.bluez.Profile1";
inline constexpr sihd::util::ZStringView gatt_manager_interface = "org.bluez.GattManager1";
inline constexpr sihd::util::ZStringView gatt_service_interface = "org.bluez.GattService1";
inline constexpr sihd::util::ZStringView gatt_characteristic_interface = "org.bluez.GattCharacteristic1";
inline constexpr sihd::util::ZStringView gatt_descriptor_interface = "org.bluez.GattDescriptor1";
inline constexpr sihd::util::ZStringView advertising_manager_interface = "org.bluez.LEAdvertisingManager1";
inline constexpr sihd::util::ZStringView advertisement_interface = "org.bluez.LEAdvertisement1";
inline constexpr sihd::util::ZStringView properties_interface = "org.freedesktop.DBus.Properties";
inline constexpr sihd::util::ZStringView object_manager_interface = "org.freedesktop.DBus.ObjectManager";
inline constexpr sihd::util::ZStringView rejected_error = "org.bluez.Error.Rejected";
inline constexpr sihd::util::ZStringView not_permitted_error = "org.bluez.Error.NotPermitted";

extern const sihd::util::Duration call_timeout;
// a remote device can take its time to answer a connection or a pairing
extern const sihd::util::Duration connect_timeout;
extern const sihd::util::Duration pair_timeout;

struct AdapterObject
{
        std::string path;
        AdapterInfo info;
};

struct DeviceObject
{
        std::string adapter_path;
        // bluez saw it advertise during this discovery
        bool seen = false;
        bool paired = false;
        bool connected = false;
        bool trusted = false;
        bool services_resolved = false;
        ScanResult result;
};

struct GattObject
{
        std::string interface;
        std::string uuid;
        uint16_t handle = 0;
        // the device of a service, the service of a characteristic, the characteristic of a descriptor
        std::string parent;
        bool primary = false;
        uint8_t properties = 0;
};

struct ManagedObjects
{
        // sorted by path: the adapter index follows hci0, hci1...
        std::vector<AdapterObject> adapters;
        // keyed by object path
        std::map<std::string, DeviceObject> devices;
        std::map<std::string, GattObject> gatt;
};

sihd::util::Result<dbus::Connection> open_system_bus();
sihd::util::Result<ManagedObjects> managed_objects(dbus::Connection & connection);
sihd::util::Result<ManagedObjects> open_managed_objects();
// interfaces is the a{sa{sv}} of one object
sihd::util::Result<void> read_object(ManagedObjects & objects, const std::string & path, dbus::Iter & interfaces);
sihd::util::Result<void> read_device_properties(DeviceObject & device, dbus::Iter & properties);

sihd::util::Result<std::string> adapter_path(const ManagedObjects & objects, size_t index);
sihd::util::Result<std::string> adapter_path(dbus::Connection & connection, size_t index);
std::string device_path(const std::string & adapter_path, const std::string & address);
DeviceInfo device_info(const DeviceObject & device);

uint8_t properties_of(const std::vector<std::string> & flags);
std::vector<std::string> flags_of(uint8_t properties);

// the error of a failed bluez call, its code following the org.bluez.Error name
sihd::util::Error error_of(const sihd::util::Error & error);

using Arguments = std::function<sihd::util::Result<void>(dbus::Appender &)>;

sihd::util::Result<dbus::Message> method_call(sihd::util::ZStringView path,
                                              sihd::util::ZStringView interface,
                                              sihd::util::ZStringView member,
                                              const Arguments & arguments);
sihd::util::Result<dbus::Message> call(dbus::Connection & connection,
                                       sihd::util::ZStringView path,
                                       sihd::util::ZStringView interface,
                                       sihd::util::ZStringView member,
                                       const Arguments & arguments,
                                       sihd::util::Duration timeout);
sihd::util::Result<void> set_property(dbus::Connection & connection,
                                      sihd::util::ZStringView path,
                                      sihd::util::ZStringView interface,
                                      sihd::util::ZStringView name,
                                      bool value);
sihd::util::Result<void> set_property(dbus::Connection & connection,
                                      sihd::util::ZStringView path,
                                      sihd::util::ZStringView interface,
                                      sihd::util::ZStringView name,
                                      const std::string & value);

} // namespace sihd::bt::bluez

#endif
