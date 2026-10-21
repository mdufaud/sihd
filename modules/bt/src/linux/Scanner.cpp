#include <algorithm>
#include <functional>
#include <map>
#include <optional>

#include <sihd/bt/Scanner.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::bt
{

using namespace sihd::util;
using namespace sihd::sys::linux_::dbus;

SIHD_LOGGER;

namespace
{

const std::string bluez_service = "org.bluez";
const std::string adapter_interface = "org.bluez.Adapter1";
const std::string device_interface = "org.bluez.Device1";

const std::string interfaces_added_rule = "type='signal',sender='org.bluez',"
                                          "interface='org.freedesktop.DBus.ObjectManager',member='InterfacesAdded'";
const std::string properties_changed_rule = "type='signal',sender='org.bluez',"
                                            "interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',"
                                            "arg0='org.bluez.Device1'";

const Duration call_timeout(time::sec(5));
// the loop wakes up on a signal, a stop or scan_for's deadline
const Duration max_wait(time::hours(1));

struct AdapterObject
{
        std::string path;
        std::string address;
        bool powered = false;
};

struct DeviceObject
{
        std::string adapter_path;
        bool seen = false;
        ScanResult result;
};

struct ManagedObjects
{
        std::vector<AdapterObject> adapters;
        // keyed by object path
        std::map<std::string, DeviceObject> devices;
};

template <BasicType Key>
using EntryCallback = std::function<Result<void>(const Key & key, Iter & value)>;

template <BasicType Key>
Result<void> for_each_entry(const Iter & dict, const EntryCallback<Key> & callback)
{
    auto entries = dict.recurse();
    SIHD_UNEXPECTED_RETURN(entries);
    for (; entries->type() != '\0'; entries->next())
    {
        auto entry = entries->recurse();
        SIHD_UNEXPECTED_RETURN(entry);
        auto key = entry->get<Key>();
        SIHD_UNEXPECTED_RETURN(key);
        entry->next();

        auto value = entry->type() == 'v' ? entry->recurse() : Result<Iter>(std::move(*entry));
        SIHD_UNEXPECTED_RETURN(value);
        auto done = callback(*key, *value);
        SIHD_UNEXPECTED_RETURN(done);
    }
    return {};
}

// a{qv} manufacturer data or a{sv} service data, an ay in each variant
template <BasicType Key>
Result<std::map<Key, std::vector<uint8_t>>> read_byte_map(const Iter & dict)
{
    std::map<Key, std::vector<uint8_t>> byte_map;
    auto parsed = for_each_entry<Key>(dict, [&byte_map](const Key & key, Iter & value) -> Result<void> {
        auto bytes = value.get_array<uint8_t>();
        SIHD_UNEXPECTED_RETURN(bytes);
        byte_map.insert_or_assign(key, std::move(*bytes));
        return {};
    });
    SIHD_UNEXPECTED_RETURN(parsed);
    return byte_map;
}

Result<void> read_adapter_property(AdapterObject & adapter, const std::string & key, Iter & value)
{
    if (key == "Address")
    {
        auto address = value.get<std::string>();
        SIHD_UNEXPECTED_RETURN(address);
        adapter.address = std::move(*address);
    }
    else if (key == "Powered")
    {
        auto powered = value.get<bool>();
        SIHD_UNEXPECTED_RETURN(powered);
        adapter.powered = *powered;
    }
    return {};
}

Result<void> read_device_identity(DeviceObject & device, const std::string & key, Iter & value)
{
    auto str = value.get<std::string>();
    SIHD_UNEXPECTED_RETURN(str);
    if (key == "Adapter")
        device.adapter_path = std::move(*str);
    else if (key == "Address")
        device.result.address = std::move(*str);
    else if (key == "AddressType")
        device.result.address_type = *str == "public" ? AddressType::public_address : AddressType::random;
    else if (key == "Name")
        device.result.identifier = std::move(*str);
    return {};
}

Result<void> read_device_property(DeviceObject & device, const std::string & key, Iter & value)
{
    if (key == "Adapter" || key == "Address" || key == "AddressType" || key == "Name")
        return read_device_identity(device, key, value);

    if (key == "RSSI")
    {
        auto rssi = value.get<int16_t>();
        SIHD_UNEXPECTED_RETURN(rssi);
        device.result.rssi = *rssi;
        device.seen = true;
        return {};
    }

    if (key == "TxPower")
    {
        auto tx_power = value.get<int16_t>();
        SIHD_UNEXPECTED_RETURN(tx_power);
        device.result.tx_power = *tx_power;
        return {};
    }

    if (key == "UUIDs")
    {
        auto uuids = value.get_array<std::string>();
        SIHD_UNEXPECTED_RETURN(uuids);
        device.result.uuids = std::move(*uuids);
        return {};
    }

    if (key == "ManufacturerData")
    {
        auto manufacturer_data = read_byte_map<uint16_t>(value);
        SIHD_UNEXPECTED_RETURN(manufacturer_data);
        device.result.manufacturer_data = std::move(*manufacturer_data);
        return {};
    }

    if (key == "ServiceData")
    {
        auto service_data = read_byte_map<std::string>(value);
        SIHD_UNEXPECTED_RETURN(service_data);
        device.result.service_data = std::move(*service_data);
    }
    return {};
}

Result<void> read_object(ManagedObjects & objects, const std::string & path, Iter & interfaces)
{
    return for_each_entry<std::string>(
        interfaces,
        [&](const std::string & interface, Iter & properties) -> Result<void> {
            if (interface == adapter_interface)
            {
                AdapterObject & adapter = objects.adapters.emplace_back();
                adapter.path = path;
                return for_each_entry<std::string>(properties, [&](const std::string & key, Iter & value) {
                    return read_adapter_property(adapter, key, value);
                });
            }
            if (interface == device_interface)
            {
                DeviceObject & device = objects.devices[path];
                return for_each_entry<std::string>(properties, [&](const std::string & key, Iter & value) {
                    return read_device_property(device, key, value);
                });
            }
            return {};
        });
}

// InterfacesAdded (o, a{sa{sv}}) or a Device1 PropertiesChanged (s, a{sv}, as): the device path it changed
Result<std::optional<std::string>> apply_signal(ManagedObjects & objects, const Message & message)
{
    Iter body(message);
    if (message.member() == "InterfacesAdded")
    {
        auto path = body.get<std::string>();
        SIHD_UNEXPECTED_RETURN(path);
        body.next();
        auto parsed = read_object(objects, *path, body);
        SIHD_UNEXPECTED_RETURN(parsed);
        return objects.devices.contains(*path) ? std::optional<std::string>(*path) : std::nullopt;
    }

    auto interface = body.get<std::string>();
    SIHD_UNEXPECTED_RETURN(interface);
    if (message.member() != "PropertiesChanged" || *interface != device_interface)
        return std::nullopt;
    body.next();

    const std::string path = message.path();
    DeviceObject & device = objects.devices[path];
    auto parsed = for_each_entry<std::string>(body, [&device](const std::string & key, Iter & value) {
        return read_device_property(device, key, value);
    });
    SIHD_UNEXPECTED_RETURN(parsed);
    return path;
}

Result<ManagedObjects> get_managed_objects(Connection & connection)
{
    auto request = Message::method_call(bluez_service, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    SIHD_UNEXPECTED_RETURN(request);
    auto reply = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);

    ManagedObjects objects;
    auto parsed = for_each_entry<std::string>(Iter(*reply), [&objects](const std::string & path, Iter & interfaces) {
        return read_object(objects, path, interfaces);
    });
    SIHD_UNEXPECTED_RETURN(parsed);
    std::ranges::sort(objects.adapters, {}, &AdapterObject::path);
    return objects;
}

Result<ManagedObjects> open_and_get_managed_objects()
{
    Connection connection;
    auto opened = connection.open(Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    return get_managed_objects(connection);
}

Result<void> call_adapter(Connection & connection, const std::string & adapter_path, const std::string & member)
{
    auto request = Message::method_call(bluez_service, adapter_path, adapter_interface, member);
    SIHD_UNEXPECTED_RETURN(request);
    auto reply = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

} // namespace

Result<bool> bluetooth_enabled()
{
    auto objects = open_and_get_managed_objects();
    SIHD_UNEXPECTED_RETURN(objects);
    return std::ranges::any_of(objects->adapters, &AdapterObject::powered);
}

Result<std::vector<AdapterInfo>> list_adapters()
{
    auto objects = open_and_get_managed_objects();
    SIHD_UNEXPECTED_RETURN(objects);

    std::vector<AdapterInfo> adapters;
    for (const AdapterObject & adapter : objects->adapters)
    {
        adapters.emplace_back(AdapterInfo {
            .identifier = adapter.path.substr(adapter.path.find_last_of('/') + 1),
            .address = adapter.address,
        });
    }
    return adapters;
}

bool Scanner::_scan()
{
    Connection connection;
    auto opened = connection.open(Bus::system);
    if (SIHD_UNEXPECTED_LOG(opened))
        return false;
    // subscribed before the snapshot: no change falls between both
    auto added_matched = connection.add_match(interfaces_added_rule);
    auto changed_matched = connection.add_match(properties_changed_rule);
    if (SIHD_UNEXPECTED_LOG(added_matched) || SIHD_UNEXPECTED_LOG(changed_matched))
        return false;

    auto objects = get_managed_objects(connection);
    if (SIHD_UNEXPECTED_LOG(objects))
        return false;
    if (_adapter_index >= objects->adapters.size())
    {
        SIHD_LOG(error, "Scanner: no bluetooth adapter at index {}", _adapter_index);
        return false;
    }

    const std::string adapter_path = objects->adapters[_adapter_index].path;
    auto discovering = call_adapter(connection, adapter_path, "StartDiscovery");
    if (SIHD_UNEXPECTED_LOG(discovering))
        return false;

    {
        auto lock = _waitable.guard();
        _wake_scan = [&connection] {
            (void)connection.interrupt();
        };
    }
    this->service_set_ready();
    for (const auto & [path, device] : objects->devices)
    {
        if (device.seen && device.adapter_path == adapter_path)
            this->_publish(ScanResult(device.result));
    }

    bool scanned = true;
    while (!_stop_requested)
    {
        auto wait = this->_next_wait(max_wait);
        if (!wait.has_value())
            break;
        auto message = Message::receive(connection, *wait);
        if (SIHD_UNEXPECTED_LOG(message))
        {
            scanned = false;
            break;
        }
        if (!message->has_value())
            continue;

        auto changed = apply_signal(*objects, **message);
        if (SIHD_UNEXPECTED_LOG(changed) || !changed->has_value())
            continue;
        const DeviceObject & device = objects->devices.at(**changed);
        if (device.seen && device.adapter_path == adapter_path)
            this->_publish(ScanResult(device.result));
    }

    {
        auto lock = _waitable.guard();
        _wake_scan = nullptr;
    }
    auto stopped = call_adapter(connection, adapter_path, "StopDiscovery");
    SIHD_UNEXPECTED_LOG(stopped);
    return scanned;
}

} // namespace sihd::bt
