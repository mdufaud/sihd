#include "bluez.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

namespace sihd::bt::bluez
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

const Duration call_timeout(time::sec(5));
const Duration connect_timeout(time::sec(30));
const Duration pair_timeout(time::sec(60));

namespace
{

struct ErrorName
{
        const char *name;
        ErrorCode code;
};

constexpr std::array bluez_errors = {
    ErrorName {"org.bluez.Error.NotReady", not_initialized},
    ErrorName {"org.bluez.Error.InProgress", would_block},
    ErrorName {"org.bluez.Error.AlreadyExists", already_exists},
    ErrorName {"org.bluez.Error.AlreadyConnected", already_exists},
    ErrorName {"org.bluez.Error.DoesNotExist", not_found},
    ErrorName {"org.bluez.Error.NotAvailable", not_found},
    ErrorName {"org.bluez.Error.NotPermitted", permission_denied},
    ErrorName {"org.bluez.Error.NotAuthorized", permission_denied},
    ErrorName {"org.bluez.Error.AuthenticationRejected", permission_denied},
    ErrorName {"org.bluez.Error.AuthenticationCanceled", permission_denied},
    ErrorName {"org.bluez.Error.Rejected", permission_denied},
    ErrorName {"org.bluez.Error.Canceled", permission_denied},
    ErrorName {"org.bluez.Error.NotSupported", not_supported},
    ErrorName {"org.bluez.Error.InvalidArguments", invalid_argument},
    ErrorName {"org.bluez.Error.InvalidOffset", invalid_argument},
    ErrorName {"org.bluez.Error.InvalidValueLength", invalid_argument},
    ErrorName {"org.bluez.Error.NotConnected", closed},
    ErrorName {"org.bluez.Error.AuthenticationTimeout", timeout},
};

constexpr std::array<std::pair<const char *, uint8_t>, 8> flag_bits = {{
    {"broadcast", GattCharacteristic::broadcast},
    {"read", GattCharacteristic::read},
    {"write-without-response", GattCharacteristic::write_without_response},
    {"write", GattCharacteristic::write},
    {"notify", GattCharacteristic::notify},
    {"indicate", GattCharacteristic::indicate},
    {"authenticated-signed-writes", GattCharacteristic::signed_write},
    {"extended-properties", GattCharacteristic::extended_properties},
}};

template <dbus::BasicType T>
Result<void> read_into(dbus::Iter & value, T & into)
{
    auto read = value.get<T>();
    SIHD_UNEXPECTED_RETURN(read);
    into = std::move(*read);
    return {};
}

template <dbus::BasicType T>
Result<void> read_into(dbus::Iter & value, std::optional<T> & into)
{
    auto read = value.get<T>();
    SIHD_UNEXPECTED_RETURN(read);
    into = std::move(*read);
    return {};
}

Result<void> read_adapter_property(AdapterInfo & adapter, const std::string & key, dbus::Iter & value)
{
    if (key == "Address")
        return read_into(value, adapter.address);
    if (key == "Name")
        return read_into(value, adapter.name);
    if (key == "Alias")
        return read_into(value, adapter.alias);
    if (key == "Powered")
        return read_into(value, adapter.powered);
    if (key == "Discoverable")
        return read_into(value, adapter.discoverable);
    if (key == "Pairable")
        return read_into(value, adapter.pairable);
    if (key == "Discovering")
        return read_into(value, adapter.discovering);
    return {};
}

Result<void> read_device_identity(DeviceObject & device, const std::string & key, dbus::Iter & value)
{
    if (key == "Adapter")
        return read_into(value, device.adapter_path);
    if (key == "Address")
        return read_into(value, device.result.address);
    if (key == "Name")
        return read_into(value, device.result.identifier);
    std::string address_type;
    auto read = read_into(value, address_type);
    SIHD_UNEXPECTED_RETURN(read);
    device.result.address_type = address_type == "public" ? AddressType::public_address : AddressType::random;
    return {};
}

Result<void> read_device_state(DeviceObject & device, const std::string & key, dbus::Iter & value)
{
    if (key == "Paired")
        return read_into(value, device.paired);
    if (key == "Connected")
        return read_into(value, device.connected);
    if (key == "Trusted")
        return read_into(value, device.trusted);
    if (key == "ServicesResolved")
        return read_into(value, device.services_resolved);
    return read_into(value, device.result.connectable);
}

Result<void> read_device_advertisement(DeviceObject & device, const std::string & key, dbus::Iter & value)
{
    ScanResult & result = device.result;
    if (key == "RSSI")
    {
        device.seen = true;
        return read_into(value, result.rssi);
    }
    if (key == "TxPower")
        return read_into(value, result.tx_power);
    if (key == "Appearance")
        return read_into(value, result.appearance);
    if (key == "Class")
        return read_into(value, result.class_of_device);

    if (key == "UUIDs")
    {
        auto uuids = value.get_array<std::string>();
        SIHD_UNEXPECTED_RETURN(uuids);
        result.uuids = std::move(*uuids);
        return {};
    }
    if (key == "ManufacturerData")
    {
        auto manufacturer_data = value.get_byte_map<uint16_t>();
        SIHD_UNEXPECTED_RETURN(manufacturer_data);
        result.manufacturer_data = std::move(*manufacturer_data);
        return {};
    }

    if (key == "ServiceData")
    {
        auto service_data = value.get_byte_map<std::string>();
        SIHD_UNEXPECTED_RETURN(service_data);
        result.service_data = std::move(*service_data);
    }
    return {};
}

Result<void> read_device_property(DeviceObject & device, const std::string & key, dbus::Iter & value)
{
    if (key == "Adapter" || key == "Address" || key == "AddressType" || key == "Name")
        return read_device_identity(device, key, value);
    if (key == "Paired" || key == "Connected" || key == "Trusted" || key == "ServicesResolved" || key == "Connectable")
        return read_device_state(device, key, value);
    return read_device_advertisement(device, key, value);
}

Result<void> read_gatt_property(GattObject & object, const std::string & key, dbus::Iter & value)
{
    if (key == "UUID")
        return read_into(value, object.uuid);
    if (key == "Handle")
        return read_into(value, object.handle);
    if (key == "Primary")
        return read_into(value, object.primary);
    if (key == "Device" || key == "Service" || key == "Characteristic")
        return read_into(value, object.parent);
    if (key == "Flags")
    {
        auto flags = value.get_array<std::string>();
        SIHD_UNEXPECTED_RETURN(flags);
        object.properties = properties_of(*flags);
    }
    return {};
}

bool is_gatt_interface(const std::string & interface)
{
    return interface == gatt_service_interface || interface == gatt_characteristic_interface
           || interface == gatt_descriptor_interface;
}

} // namespace

Result<void> read_device_properties(DeviceObject & device, dbus::Iter & properties)
{
    return properties.for_each_entry<std::string>(
        [&device](const std::string & key, dbus::Iter & value) { return read_device_property(device, key, value); });
}

Result<void> read_object(ManagedObjects & objects, const std::string & path, dbus::Iter & interfaces)
{
    return interfaces.for_each_entry<std::string>(
        [&](const std::string & interface, dbus::Iter & properties) -> Result<void> {
            if (interface == adapter_interface)
            {
                AdapterObject & adapter = objects.adapters.emplace_back();
                adapter.path = path;
                adapter.info.identifier = path.substr(path.find_last_of('/') + 1);
                return properties.for_each_entry<std::string>([&adapter](const std::string & key, dbus::Iter & value) {
                    return read_adapter_property(adapter.info, key, value);
                });
            }

            if (interface == device_interface)
                return read_device_properties(objects.devices[path], properties);
            if (is_gatt_interface(interface))
            {
                GattObject & object = objects.gatt[path];
                object.interface = interface;
                return properties.for_each_entry<std::string>([&object](const std::string & key, dbus::Iter & value) {
                    return read_gatt_property(object, key, value);
                });
            }
            return {};
        });
}

Result<dbus::Connection> open_system_bus()
{
    dbus::Connection connection;
    auto opened = connection.open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    return connection;
}

Result<ManagedObjects> managed_objects(dbus::Connection & connection)
{
    auto reply = call(connection, "/", object_manager_interface, "GetManagedObjects", {}, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);

    ManagedObjects objects;
    auto parsed = dbus::Iter(*reply).for_each_entry<std::string>(
        [&objects](const std::string & path, dbus::Iter & interfaces) {
            return read_object(objects, path, interfaces);
        });
    SIHD_UNEXPECTED_RETURN(parsed);
    std::ranges::sort(objects.adapters, {}, &AdapterObject::path);
    return objects;
}

Result<ManagedObjects> open_managed_objects()
{
    auto connection = open_system_bus();
    SIHD_UNEXPECTED_RETURN(connection);
    return managed_objects(*connection);
}

Result<std::string> adapter_path(const ManagedObjects & objects, size_t index)
{
    if (index >= objects.adapters.size())
        return std::unexpected(Error(not_found, "no bluetooth adapter at index {}", index));
    return objects.adapters[index].path;
}

Result<std::string> adapter_path(dbus::Connection & connection, size_t index)
{
    auto objects = managed_objects(connection);
    SIHD_UNEXPECTED_RETURN(objects);
    return adapter_path(*objects, index);
}

std::string device_path(const std::string & adapter_path, const std::string & address)
{
    std::string node = str::to_upper_copy(address);
    std::ranges::replace(node, ':', '_');
    return adapter_path + "/dev_" + node;
}

DeviceInfo device_info(const DeviceObject & device)
{
    return DeviceInfo {
        .address = device.result.address,
        .address_type = device.result.address_type,
        .name = device.result.identifier,
        .paired = device.paired,
        .connected = device.connected,
        .trusted = device.trusted,
        .appearance = device.result.appearance,
        .class_of_device = device.result.class_of_device,
        .uuids = device.result.uuids,
    };
}

uint8_t properties_of(const std::vector<std::string> & flags)
{
    uint8_t properties = 0;
    for (const auto & [flag, bit] : flag_bits)
    {
        if (std::ranges::find(flags, flag) != flags.end())
            properties |= bit;
    }
    return properties;
}

std::vector<std::string> flags_of(uint8_t properties)
{
    std::vector<std::string> flags;
    for (const auto & [flag, bit] : flag_bits)
    {
        if ((properties & bit) != 0)
            flags.emplace_back(flag);
    }
    return flags;
}

Error error_of(const Error & error)
{
    for (const ErrorName & known : bluez_errors)
    {
        if (dbus::has_error_name(error, known.name))
            return Error(known.code, error.message);
    }
    return error;
}

Result<dbus::Message>
    method_call(ZStringView path, ZStringView interface, ZStringView member, const Arguments & arguments)
{
    dbus::Message request;
    auto built = request.method_call(service, path, interface, member);
    SIHD_UNEXPECTED_RETURN(built);
    if (arguments)
    {
        dbus::Appender body(request);
        auto filled = arguments(body);
        SIHD_UNEXPECTED_RETURN(filled);
    }
    return request;
}

Result<dbus::Message> call(dbus::Connection & connection,
                           ZStringView path,
                           ZStringView interface,
                           ZStringView member,
                           const Arguments & arguments,
                           Duration timeout)
{
    auto request = method_call(path, interface, member, arguments);
    SIHD_UNEXPECTED_RETURN(request);
    return request->call(connection, timeout).transform_error(error_of);
}

Result<void>
    set_property(dbus::Connection & connection, ZStringView path, ZStringView interface, ZStringView name, bool value)
{
    return dbus::set_property(connection, service, path, interface, name, value, call_timeout)
        .transform_error(error_of);
}

Result<void> set_property(dbus::Connection & connection,
                          ZStringView path,
                          ZStringView interface,
                          ZStringView name,
                          const std::string & value)
{
    return dbus::set_property(connection, service, path, interface, name, value, call_timeout)
        .transform_error(error_of);
}

} // namespace sihd::bt::bluez
