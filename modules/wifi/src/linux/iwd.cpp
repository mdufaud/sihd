#include <algorithm>
#include <map>
#include <set>

#include <sihd/util/Error.hpp>

#include "../connect.hpp"
#include "backends.hpp"

using namespace sihd::util;

namespace sihd::wifi::iwd
{

namespace
{

constexpr ZStringView service = "net.connman.iwd";
constexpr ZStringView manager_path = "/net/connman/iwd";
constexpr ZStringView device_interface = "net.connman.iwd.Device";
constexpr ZStringView station_interface = "net.connman.iwd.Station";
constexpr ZStringView network_interface = "net.connman.iwd.Network";
constexpr ZStringView known_interface = "net.connman.iwd.KnownNetwork";
constexpr ZStringView agent_manager_interface = "net.connman.iwd.AgentManager";
constexpr ZStringView agent_interface = "net.connman.iwd.Agent";
constexpr ZStringView agent_path = "/agent";

struct NetworkObject
{
        std::string path;
        std::string name;
        std::string type;
        std::string device;
        std::string known;
};

struct KnownObject
{
        std::string path;
        std::string name;
        std::string type;
};

struct Objects
{
        std::map<std::string, std::string> station_interface_names;
        std::vector<NetworkObject> networks;
        std::vector<KnownObject> known;
};

using Strings = std::map<std::string, std::string>;

State to_state(const std::string & station_state)
{
    if (station_state == "connected" || station_state == "roaming")
        return State::connected;
    if (station_state == "connecting")
        return State::connecting;
    if (station_state == "disconnecting")
        return State::disconnecting;
    return State::disconnected;
}

Result<Strings> string_properties(dbus::Iter & properties)
{
    Strings strings;
    auto read = properties.for_each_entry<std::string>(
        [&strings](const std::string & name, dbus::Iter & value) -> Result<void> {
            if (value.type() != 's' && value.type() != 'o')
                return {};
            auto text = value.get<std::string>();
            SIHD_UNEXPECTED_RETURN(text);
            strings.insert_or_assign(name, std::move(*text));
            return {};
        });
    SIHD_UNEXPECTED_RETURN(read);
    return strings;
}

std::string value_of(const Strings & strings, const std::string & name)
{
    const auto found = strings.find(name);
    return found == strings.end() ? std::string() : found->second;
}

Result<void> read_interface(Objects & objects,
                            Strings & device_names,
                            std::set<std::string> & stations,
                            const std::string & path,
                            const std::string & interface,
                            dbus::Iter & properties)
{
    if (interface == station_interface)
    {
        stations.insert(path);
        return {};
    }
    if (interface != device_interface && interface != network_interface && interface != known_interface)
        return {};

    auto strings = string_properties(properties);
    SIHD_UNEXPECTED_RETURN(strings);
    if (interface == device_interface)
        device_names.insert_or_assign(path, value_of(*strings, "Name"));
    else if (interface == network_interface)
    {
        objects.networks.emplace_back(NetworkObject {
            .path = path,
            .name = value_of(*strings, "Name"),
            .type = value_of(*strings, "Type"),
            .device = value_of(*strings, "Device"),
            .known = value_of(*strings, "KnownNetwork"),
        });
    }
    else
    {
        objects.known.emplace_back(KnownObject {
            .path = path,
            .name = value_of(*strings, "Name"),
            .type = value_of(*strings, "Type"),
        });
    }
    return {};
}

Result<Objects> managed_objects(dbus::Connection & connection)
{
    auto request = dbus::Message::method_call(service, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    SIHD_UNEXPECTED_RETURN(request);
    auto reply = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);

    Objects objects;
    Strings device_names;
    std::set<std::string> stations;
    auto parsed = dbus::Iter(*reply).for_each_entry<std::string>(
        [&](const std::string & path, dbus::Iter & interfaces) {
            return interfaces.for_each_entry<std::string>([&](const std::string & interface, dbus::Iter & properties) {
                return read_interface(objects, device_names, stations, path, interface, properties);
            });
        });
    SIHD_UNEXPECTED_RETURN(parsed);
    for (const std::string & station : stations)
        objects.station_interface_names.insert_or_assign(station, value_of(device_names, station));
    return objects;
}

Security type_security(const std::string & type)
{
    if (type == "open")
        return Security::open;
    if (type == "psk")
        return Security::personal;
    if (type == "8021x" || type == "hotspot")
        return Security::enterprise;
    if (type == "wep")
        return Security::wep;
    return Security::unknown;
}

Result<void> call_simple(dbus::Connection & connection, ZStringView path, ZStringView interface, ZStringView member)
{
    auto request = dbus::Message::method_call(service, path, interface, member);
    SIHD_UNEXPECTED_RETURN(request);
    auto reply = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

// the reply, for its sender: the unique name iwd answers from
Result<dbus::Message> call_agent_manager(dbus::Connection & connection, ZStringView member)
{
    auto request = dbus::Message::method_call(service, manager_path, agent_manager_interface, member);
    SIHD_UNEXPECTED_RETURN(request);
    auto path_added = request->add_object_path(agent_path);
    SIHD_UNEXPECTED_RETURN(path_added);
    return request->call(connection, call_timeout);
}

// the passphrase must go to iwd only: any other connection may call the agent too
Result<void> answer_agent(dbus::Connection & connection,
                          const dbus::Message & call,
                          const std::string & owner,
                          const std::optional<std::string> & passphrase)
{
    if (call.path() != agent_path || call.interface() != agent_interface || call.member() != "RequestPassphrase")
        return {};
    const bool trusted = passphrase.has_value() && call.sender() == owner;
    auto answer = trusted ? call.method_return()
                          : call.error_return("net.connman.iwd.Agent.Error.Canceled", "this agent only answers iwd");
    SIHD_UNEXPECTED_RETURN(answer);
    if (trusted)
    {
        auto added = answer->add(*passphrase);
        SIHD_UNEXPECTED_RETURN(added);
    }
    return answer->send(connection);
}

Result<std::optional<std::string>> changed_text(const dbus::Message & message, const std::string & name)
{
    auto changed = message.changed_property(station_interface, name);
    SIHD_UNEXPECTED_RETURN(changed);
    if (!changed->has_value())
        return std::nullopt;
    return (*changed)->get<std::string>();
}

// nullopt unless Scanning changed, true once a scan is over
Result<std::optional<bool>> scan_over(const dbus::Message & message)
{
    auto changed = message.changed_property(station_interface, "Scanning");
    SIHD_UNEXPECTED_RETURN(changed);
    if (!changed->has_value())
        return std::nullopt;
    auto scanning = (*changed)->get<bool>();
    SIHD_UNEXPECTED_RETURN(scanning);
    return !*scanning;
}

// Connect blocks until connected: the agent must be served meanwhile
Result<void> connect_network(dbus::Connection & connection,
                             const NetworkObject & network,
                             const std::optional<std::string> & passphrase,
                             Duration timeout)
{
    std::string owner;
    if (passphrase.has_value())
    {
        auto registered = call_agent_manager(connection, "RegisterAgent");
        SIHD_UNEXPECTED_RETURN(registered);
        owner = registered->sender();
    }

    auto request = dbus::Message::method_call(service, network.path, network_interface, "Connect");
    SIHD_UNEXPECTED_RETURN(request);
    auto sent = request->send(connection);
    SIHD_UNEXPECTED_RETURN(sent);
    auto connected = dbus::Message::receive_until(
        connection,
        timeout,
        [&connection, &owner, &passphrase](const dbus::Message & message) -> Result<bool> {
            switch (message.type())
            {
                case dbus::Message::Type::method_call:
                {
                    auto answered = answer_agent(connection, message, owner, passphrase);
                    SIHD_UNEXPECTED_RETURN(answered);
                    return false;
                }
                case dbus::Message::Type::method_return:
                    return true;
                case dbus::Message::Type::error:
                    return std::unexpected(message.to_error());
                default:
                    return false;
            }
        });
    if (passphrase.has_value())
        (void)call_agent_manager(connection, "UnregisterAgent");
    SIHD_UNEXPECTED_RETURN_CTX(connected, "connecting to {}", network.name);
    return {};
}

} // namespace

Result<std::optional<std::string>> detect(dbus::Connection & connection, const std::string & interface_name)
{
    auto running = connection.name_has_owner(service);
    SIHD_UNEXPECTED_RETURN(running);
    if (!*running)
        return std::nullopt;
    auto objects = managed_objects(connection);
    SIHD_UNEXPECTED_RETURN(objects);
    const auto found = std::ranges::find(objects->station_interface_names, interface_name, [](const auto & station) {
        return station.second;
    });
    if (found == objects->station_interface_names.end())
        return std::nullopt;
    return found->first;
}

Result<State> state(dbus::Connection & connection, const std::string & station)
{
    auto station_state = dbus::get_property<std::string>(connection,
                                                         service,
                                                         station,
                                                         station_interface,
                                                         "State",
                                                         call_timeout);
    SIHD_UNEXPECTED_RETURN(station_state);
    return to_state(*station_state);
}

Result<void> scan(dbus::Connection & connection, const std::string & station, Duration timeout)
{
    auto matched = connection.add_match(dbus::properties_changed_rule(service, station, station_interface));
    SIHD_UNEXPECTED_RETURN(matched);
    auto requested = call_simple(connection, station, station_interface, "Scan");
    const bool already_scanning = !requested && dbus::has_error_name(requested.error(), "net.connman.iwd.Busy");
    if (!already_scanning)
        SIHD_UNEXPECTED_RETURN(requested);

    auto scanned = dbus::Message::receive_until(connection,
                                                timeout,
                                                [&station](const dbus::Message & message) -> Result<bool> {
                                                    if (message.path() != station)
                                                        return false;
                                                    auto over = scan_over(message);
                                                    SIHD_UNEXPECTED_RETURN(over);
                                                    return over->value_or(false);
                                                });
    SIHD_UNEXPECTED_RETURN_CTX(scanned, "iwd scan");
    return {};
}

Result<void> connect(dbus::Connection & connection,
                     const std::string & station,
                     const std::string & ssid,
                     const std::optional<std::string> & passphrase,
                     Duration timeout)
{
    auto objects = managed_objects(connection);
    SIHD_UNEXPECTED_RETURN(objects);
    const auto network = std::ranges::find_if(objects->networks, [&station, &ssid](const NetworkObject & candidate) {
        return candidate.device == station && candidate.name == ssid;
    });
    if (network == objects->networks.end())
        return std::unexpected(Error(ErrorCode::not_found, "iwd did not see {}", ssid));
    const Target target {.security = type_security(network->type), .known = !network->known.empty()};
    auto acceptable = check_target(ssid, target, passphrase);
    SIHD_UNEXPECTED_RETURN(acceptable);
    return connect_network(connection, *network, passphrase, timeout);
}

Result<void> disconnect(dbus::Connection & connection, const std::string & station)
{
    auto disconnected = call_simple(connection, station, station_interface, "Disconnect");
    if (!disconnected && dbus::has_error_name(disconnected.error(), "net.connman.iwd.NotConnected"))
        return {};
    return disconnected;
}

Result<std::vector<KnownNetwork>> known_networks(dbus::Connection & connection, const std::string &)
{
    auto objects = managed_objects(connection);
    SIHD_UNEXPECTED_RETURN(objects);
    std::vector<KnownNetwork> known;
    for (const KnownObject & object : objects->known)
        known.emplace_back(KnownNetwork {.ssid = object.name, .security = type_security(object.type)});
    return known;
}

Result<void> forget(dbus::Connection & connection, const std::string &, const std::string & ssid)
{
    auto objects = managed_objects(connection);
    SIHD_UNEXPECTED_RETURN(objects);
    const auto known = std::ranges::find(objects->known, ssid, &KnownObject::name);
    if (known == objects->known.end())
        return std::unexpected(Error(ErrorCode::not_found, "no known network {}", ssid));
    return call_simple(connection, known->path, known_interface, "Forget");
}

std::vector<std::string> monitor_rules(const std::string & station)
{
    return {dbus::properties_changed_rule(service, station, station_interface)};
}

Result<std::vector<Monitor::Event>> events_of(const dbus::Message & message, const std::string & station)
{
    std::vector<Monitor::Event> events;
    if (message.path() != station)
        return events;
    auto station_state = changed_text(message, "State");
    SIHD_UNEXPECTED_RETURN(station_state);
    if (station_state->has_value())
        events.emplace_back(
            Monitor::Event {.type = Monitor::Event::Type::state_changed, .state = to_state(**station_state)});
    auto over = scan_over(message);
    SIHD_UNEXPECTED_RETURN(over);
    if (over->value_or(false))
        events.emplace_back(Monitor::Event {.type = Monitor::Event::Type::scan_done, .state = State::disconnected});
    return events;
}

} // namespace sihd::wifi::iwd
