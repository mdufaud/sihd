#include <optional>

#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "bluez.hpp"

namespace sihd::bt
{

using namespace sihd::util;
namespace dbus = sihd::sys::linux_::dbus;

SIHD_LOGGER;

namespace
{

constexpr ZStringView interfaces_added_rule = "type='signal',sender='org.bluez',"
                                              "interface='org.freedesktop.DBus.ObjectManager',member='InterfacesAdded'";
constexpr ZStringView
    properties_changed_rule = "type='signal',sender='org.bluez',"
                              "interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',"
                              "arg0='org.bluez.Device1'";

// the loop wakes up on a signal, a stop or scan_for's deadline
const Duration max_wait(time::hours(1));

std::string transport_of(Transport transport)
{
    switch (transport)
    {
        case Transport::le:
            return "le";
        case Transport::classic:
            return "bredr";
        default:
            return "auto";
    }
}

Result<void> add_filter(dbus::Appender & to, const ScanFilter & filter, bool duplicates)
{
    auto entries = to.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(entries);
    auto transport = entries->add_entry("Transport", "s", [&filter](dbus::Appender & value) {
        return value.add(transport_of(filter.transport));
    });
    SIHD_UNEXPECTED_RETURN(transport);
    if (!filter.uuids.empty())
    {
        auto uuids = entries->add_entry("UUIDs", "as", [&filter](dbus::Appender & value) {
            return value.add_array(filter.uuids);
        });
        SIHD_UNEXPECTED_RETURN(uuids);
    }
    if (filter.min_rssi.has_value())
    {
        auto rssi = entries->add_entry("RSSI", "n", [&filter](dbus::Appender & value) {
            return value.add(*filter.min_rssi);
        });
        SIHD_UNEXPECTED_RETURN(rssi);
    }
    if (duplicates)
    {
        // bluez repeats an unchanged manufacturer or service data only then
        auto duplicated = entries->add_entry("DuplicateData", "b", [](dbus::Appender & value) {
            return value.add(true);
        });
        SIHD_UNEXPECTED_RETURN(duplicated);
    }
    return to.close(std::move(*entries));
}

// InterfacesAdded (o, a{sa{sv}}) or a Device1 PropertiesChanged (s, a{sv}, as): the device path it changed
Result<std::optional<std::string>> apply_signal(bluez::ManagedObjects & objects, const dbus::Message & message)
{
    dbus::Iter body(message);
    if (message.member() == "InterfacesAdded")
    {
        auto path = body.get<std::string>();
        SIHD_UNEXPECTED_RETURN(path);
        body.next();
        auto parsed = bluez::read_object(objects, *path, body);
        SIHD_UNEXPECTED_RETURN(parsed);
        return objects.devices.contains(*path) ? std::optional<std::string>(*path) : std::nullopt;
    }

    auto interface = body.get<std::string>();
    SIHD_UNEXPECTED_RETURN(interface);
    if (message.member() != "PropertiesChanged" || *interface != bluez::device_interface)
        return std::nullopt;
    body.next();

    const std::string path = message.path();
    auto parsed = bluez::read_device_properties(objects.devices[path], body);
    SIHD_UNEXPECTED_RETURN(parsed);
    return path;
}

Result<void> call_adapter(dbus::Connection & connection,
                          const std::string & adapter_path,
                          ZStringView member,
                          const bluez::Arguments & arguments = {})
{
    auto reply = bluez::call(connection,
                             adapter_path,
                             bluez::adapter_interface,
                             member,
                             arguments,
                             bluez::call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

} // namespace

bool Scanner::_scan()
{
    dbus::Connection connection;
    auto opened = connection.open(dbus::Bus::system);
    if (SIHD_UNEXPECTED_LOG(opened))
        return false;
    // subscribed before the snapshot: no change falls between both
    auto added_matched = connection.add_match(interfaces_added_rule);
    auto changed_matched = connection.add_match(properties_changed_rule);
    if (SIHD_UNEXPECTED_LOG(added_matched) || SIHD_UNEXPECTED_LOG(changed_matched))
        return false;

    auto objects = bluez::managed_objects(connection);
    if (SIHD_UNEXPECTED_LOG(objects))
        return false;
    auto found_path = bluez::adapter_path(*objects, _adapter_index);
    if (SIHD_UNEXPECTED_LOG(found_path))
        return false;
    const std::string adapter_path = *found_path;

    auto filtered = call_adapter(connection, adapter_path, "SetDiscoveryFilter", [this](dbus::Appender & to) {
        return add_filter(to, _filter, _lost_timeout > Duration(0));
    });
    if (SIHD_UNEXPECTED_LOG(filtered))
        return false;
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
    dbus::Message message;
    while (!this->service_stop_requested())
    {
        this->_forget_lost();
        auto wait = this->_next_wait(max_wait);
        if (!wait.has_value())
            break;
        auto received = message.receive(connection, *wait);
        if (SIHD_UNEXPECTED_LOG(received))
        {
            scanned = false;
            break;
        }
        if (!*received)
            continue;

        auto changed = apply_signal(*objects, message);
        if (SIHD_UNEXPECTED_LOG(changed) || !changed->has_value())
            continue;
        const bluez::DeviceObject & device = objects->devices.at(**changed);
        if (device.seen && device.adapter_path == adapter_path)
            this->_publish(ScanResult(device.result));
    }

    {
        auto lock = _waitable.guard();
        _wake_scan = nullptr;
    }
    // the filter belongs to this client: bluez drops it with the connection
    auto stopped = call_adapter(connection, adapter_path, "StopDiscovery");
    SIHD_UNEXPECTED_LOG(stopped);
    return scanned;
}

} // namespace sihd::bt
