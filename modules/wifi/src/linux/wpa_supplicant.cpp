#include <algorithm>

#include <sihd/util/Error.hpp>
#include <sihd/util/str.hpp>
#include <sihd/wifi/scan.hpp>

#include "../connect.hpp"
#include "backends.hpp"

using namespace sihd::util;

namespace sihd::wifi::wpa_supplicant
{

namespace
{

constexpr ZStringView service = "fi.w1.wpa_supplicant1";
constexpr ZStringView root_path = "/fi/w1/wpa_supplicant1";
constexpr ZStringView root_interface = "fi.w1.wpa_supplicant1";
constexpr ZStringView interface_interface = "fi.w1.wpa_supplicant1.Interface";
constexpr ZStringView network_interface = "fi.w1.wpa_supplicant1.Network";
constexpr ZStringView bss_interface = "fi.w1.wpa_supplicant1.BSS";
// sae must have frame protection, kept optional so psk-only access points still work
constexpr ZStringView personal_key_mgmt = "WPA-PSK SAE";
constexpr uint32_t optional_frame_protection = 1;
constexpr uint32_t required_frame_protection = 2;

struct NetworkObject
{
        std::string path;
        std::string ssid;
        std::string key_mgmt;
};

struct BssObject
{
        std::string ssid;
        std::vector<uint8_t> information_elements;
        bool privacy = false;
};

State to_state(const std::string & interface_state)
{
    if (interface_state == "completed")
        return State::connected;
    if (interface_state == "authenticating" || interface_state == "associating" || interface_state == "associated"
        || interface_state == "4way_handshake" || interface_state == "group_handshake")
        return State::connecting;
    return State::disconnected;
}

// wpa_supplicant always quotes a text ssid, a raw one comes in hex
std::string config_ssid(const std::string & value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return value.substr(1, value.size() - 2);
    auto bytes = str::from_hex(value);
    if (!bytes.has_value())
        return value;
    return std::string(bytes->begin(), bytes->end());
}

Security key_mgmt_security(const std::string & key_mgmt)
{
    if (key_mgmt.find("PSK") != std::string::npos || key_mgmt.find("SAE") != std::string::npos)
        return Security::personal;
    if (key_mgmt.find("EAP") != std::string::npos || key_mgmt.find("IEEE8021X") != std::string::npos)
        return Security::enterprise;
    if (key_mgmt.find("OWE") != std::string::npos)
        return Security::owe;
    return Security::open;
}

Result<NetworkObject> read_network(const dbus::Message & reply, const std::string & path)
{
    auto properties = dbus::Iter(reply).recurse();
    SIHD_UNEXPECTED_RETURN(properties);

    NetworkObject network;
    network.path = path;
    auto read = properties->for_each_entry<std::string>(
        [&network](const std::string & name, dbus::Iter & value) -> Result<void> {
            if (name != "ssid" && name != "key_mgmt")
                return {};
            auto text = value.get<std::string>();
            SIHD_UNEXPECTED_RETURN(text);
            if (name == "ssid")
                network.ssid = config_ssid(*text);
            else
                network.key_mgmt = std::move(*text);
            return {};
        });
    SIHD_UNEXPECTED_RETURN(read);
    return network;
}

Result<std::vector<NetworkObject>> networks(dbus::Connection & connection, const std::string & interface)
{
    auto paths = dbus::get_array_property<std::string>(connection,
                                                       service,
                                                       interface,
                                                       interface_interface,
                                                       "Networks",
                                                       call_timeout);
    SIHD_UNEXPECTED_RETURN(paths);
    std::vector<dbus::Message> requests;
    for (const std::string & path : *paths)
    {
        auto request = dbus::Message::properties_get(service, path, network_interface, "Properties");
        SIHD_UNEXPECTED_RETURN(request);
        requests.emplace_back(std::move(*request));
    }
    auto replies = dbus::Message::call_all(connection, requests, call_timeout);
    SIHD_UNEXPECTED_RETURN(replies);

    std::vector<NetworkObject> networks;
    for (size_t index = 0; index < paths->size(); ++index)
    {
        auto network = read_network((*replies)[index], (*paths)[index]);
        SIHD_UNEXPECTED_RETURN(network);
        networks.emplace_back(std::move(*network));
    }
    return networks;
}

Result<std::vector<NetworkObject>>
    networks_of(dbus::Connection & connection, const std::string & interface, const std::string & ssid)
{
    auto found = networks(connection, interface);
    SIHD_UNEXPECTED_RETURN(found);
    std::erase_if(*found, [&ssid](const NetworkObject & network) { return network.ssid != ssid; });
    return found;
}

Result<void> read_bss_value(BssObject & bss, const std::string & name, dbus::Iter & value)
{
    if (name == "SSID" || name == "IEs")
    {
        auto bytes = value.get_array<uint8_t>();
        SIHD_UNEXPECTED_RETURN(bytes);
        if (name == "SSID")
            bss.ssid.assign(bytes->begin(), bytes->end());
        else
            bss.information_elements = std::move(*bytes);
    }
    else if (name == "Privacy")
    {
        auto privacy = value.get<bool>();
        SIHD_UNEXPECTED_RETURN(privacy);
        bss.privacy = *privacy;
    }
    return {};
}

// nullopt unless wpa_supplicant heard that ssid, its security otherwise
Result<std::optional<Security>>
    heard_security(dbus::Connection & connection, const std::string & interface, const std::string & ssid)
{
    auto paths = dbus::get_array_property<std::string>(connection,
                                                       service,
                                                       interface,
                                                       interface_interface,
                                                       "BSSs",
                                                       call_timeout);
    SIHD_UNEXPECTED_RETURN(paths);
    std::vector<dbus::Message> requests;
    for (const std::string & path : *paths)
    {
        auto request = dbus::Message::properties_get_all(service, path, bss_interface);
        SIHD_UNEXPECTED_RETURN(request);
        requests.emplace_back(std::move(*request));
    }
    auto replies = dbus::Message::call_all(connection, requests, call_timeout);
    SIHD_UNEXPECTED_RETURN(replies);

    for (const dbus::Message & reply : *replies)
    {
        BssObject bss;
        auto read = dbus::Iter(reply).for_each_entry<std::string>(
            [&bss](const std::string & name, dbus::Iter & value) { return read_bss_value(bss, name, value); });
        SIHD_UNEXPECTED_RETURN(read);
        if (bss.ssid == ssid)
            return security_of(ArrUByteView(bss.information_elements.data(), bss.information_elements.size()),
                               bss.privacy);
    }
    return std::nullopt;
}

Result<void> call_with_path(dbus::Connection & connection, ZStringView interface, ZStringView member, ZStringView path)
{
    auto request = dbus::Message::method_call(service, interface, interface_interface, member);
    SIHD_UNEXPECTED_RETURN(request);
    auto path_added = request->add_object_path(path);
    SIHD_UNEXPECTED_RETURN(path_added);
    auto reply = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

Result<void> add_text_entry(dbus::Appender & dict, ZStringView key, ZStringView value)
{
    return dict.add_entry(key, "s", [&value](dbus::Appender & variant) { return variant.add(value); });
}

Result<void> add_frame_protection(dbus::Appender & dict, uint32_t frame_protection)
{
    return dict.add_entry("ieee80211w", "u", [frame_protection](dbus::Appender & variant) {
        return variant.add(frame_protection);
    });
}

// wpa_supplicant always quotes string values: the ssid must go raw as bytes
Result<void> fill_network(dbus::Appender & dict,
                          const std::string & ssid,
                          Security security,
                          const std::optional<std::string> & passphrase)
{
    auto ssid_added = dict.add_entry("ssid", "ay", [&ssid](dbus::Appender & variant) {
        return variant.add_array(std::vector<uint8_t>(ssid.begin(), ssid.end()));
    });
    SIHD_UNEXPECTED_RETURN(ssid_added);
    if (security == Security::owe)
    {
        auto key_mgmt_added = add_text_entry(dict, "key_mgmt", "OWE");
        SIHD_UNEXPECTED_RETURN(key_mgmt_added);
        return add_frame_protection(dict, required_frame_protection);
    }
    if (!passphrase.has_value())
        return add_text_entry(dict, "key_mgmt", "NONE");

    auto psk_added = add_text_entry(dict, "psk", *passphrase);
    SIHD_UNEXPECTED_RETURN(psk_added);
    auto key_mgmt_added = add_text_entry(dict, "key_mgmt", personal_key_mgmt);
    SIHD_UNEXPECTED_RETURN(key_mgmt_added);
    return add_frame_protection(dict, optional_frame_protection);
}

Result<std::string> add_network(dbus::Connection & connection,
                                const std::string & interface,
                                const std::string & ssid,
                                Security security,
                                const std::optional<std::string> & passphrase)
{
    auto request = dbus::Message::method_call(service, interface, interface_interface, "AddNetwork");
    SIHD_UNEXPECTED_RETURN(request);
    dbus::Appender body(*request);
    auto dict = body.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(dict);
    auto filled = fill_network(*dict, ssid, security, passphrase);
    SIHD_UNEXPECTED_RETURN(filled);
    auto dict_closed = body.close(std::move(*dict));
    SIHD_UNEXPECTED_RETURN(dict_closed);
    auto reply = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return dbus::Iter(*reply).get<std::string>();
}

Result<std::optional<std::string>> changed_state(const dbus::Message & message)
{
    auto changed = message.changed_property(interface_interface, "State");
    SIHD_UNEXPECTED_RETURN(changed);
    if (!changed->has_value())
        return std::nullopt;
    return (*changed)->get<std::string>();
}

// a refused passphrase never fails a call: the interface goes back to disconnected
Result<void> wait_completed(dbus::Connection & connection,
                            const std::string & interface,
                            const std::string & ssid,
                            Duration timeout)
{
    bool attempted = false;
    auto completed = dbus::Message::receive_until(
        connection,
        timeout,
        [&interface, &attempted](const dbus::Message & message) -> Result<bool> {
            if (message.path() != interface)
                return false;
            auto state = changed_state(message);
            SIHD_UNEXPECTED_RETURN(state);
            if (!state->has_value())
                return false;
            if (attempted && **state == "disconnected")
                return std::unexpected(Error(ErrorCode::io_error, "wpa_supplicant disconnected"));
            attempted = attempted || to_state(**state) == State::connecting;
            return **state == "completed";
        });
    SIHD_UNEXPECTED_RETURN_CTX(completed, "connecting to {}", ssid);
    return {};
}

bool is_scan_done(const dbus::Message & message, const std::string & interface)
{
    return message.type() == dbus::Message::Type::signal
           && message.path() == interface && message.interface() == interface_interface
           && message.member() == "ScanDone";
}

} // namespace

Result<std::optional<std::string>> detect(dbus::Connection & connection, const std::string & interface_name)
{
    auto running = connection.name_has_owner(service);
    SIHD_UNEXPECTED_RETURN(running);
    if (!*running)
        return std::nullopt;

    auto request = dbus::Message::method_call(service, root_path, root_interface, "GetInterface");
    SIHD_UNEXPECTED_RETURN(request);
    auto name_added = request->add(interface_name);
    SIHD_UNEXPECTED_RETURN(name_added);
    auto reply = request->call(connection, call_timeout);
    if (!reply && dbus::has_error_name(reply.error(), "fi.w1.wpa_supplicant1.InterfaceUnknown"))
        return std::nullopt;
    SIHD_UNEXPECTED_RETURN(reply);
    auto interface = dbus::Iter(*reply).get<std::string>();
    SIHD_UNEXPECTED_RETURN(interface);
    return *interface;
}

Result<State> state(dbus::Connection & connection, const std::string & interface)
{
    auto interface_state = dbus::get_property<std::string>(connection,
                                                           service,
                                                           interface,
                                                           interface_interface,
                                                           "State",
                                                           call_timeout);
    SIHD_UNEXPECTED_RETURN(interface_state);
    return to_state(*interface_state);
}

Result<void> scan(dbus::Connection & connection, const std::string & interface, Duration timeout)
{
    auto matched = connection.add_match(dbus::signal_rule(service, interface, interface_interface, "ScanDone"));
    SIHD_UNEXPECTED_RETURN(matched);
    auto request = dbus::Message::method_call(service, interface, interface_interface, "Scan");
    SIHD_UNEXPECTED_RETURN(request);
    dbus::Appender body(*request);
    auto options = body.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(options);
    auto type_added = add_text_entry(*options, "Type", "active");
    SIHD_UNEXPECTED_RETURN(type_added);
    auto options_closed = body.close(std::move(*options));
    SIHD_UNEXPECTED_RETURN(options_closed);
    auto requested = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(requested);

    auto done = dbus::Message::receive_until(
        connection,
        timeout,
        [&interface](const dbus::Message & message) -> Result<bool> {
            if (!is_scan_done(message, interface))
                return false;
            auto success = dbus::Iter(message).get<bool>();
            SIHD_UNEXPECTED_RETURN(success);
            if (!*success)
                return std::unexpected(Error(ErrorCode::io_error, "ScanDone reported a failure"));
            return true;
        });
    SIHD_UNEXPECTED_RETURN_CTX(done, "wpa_supplicant scan");
    return {};
}

Result<void> connect(dbus::Connection & connection,
                     const std::string & interface,
                     const std::string & ssid,
                     const std::optional<std::string> & passphrase,
                     Duration timeout)
{
    auto security = heard_security(connection, interface, ssid);
    SIHD_UNEXPECTED_RETURN(security);
    if (!security->has_value())
        return std::unexpected(Error(ErrorCode::not_found, "wpa_supplicant did not see {}", ssid));
    auto known = networks_of(connection, interface, ssid);
    SIHD_UNEXPECTED_RETURN(known);
    auto acceptable = check_target(ssid, Target {.security = **security, .known = !known->empty()}, passphrase);
    SIHD_UNEXPECTED_RETURN(acceptable);

    std::optional<std::string> added;
    std::string selected;
    if (known->empty())
    {
        auto network = add_network(connection, interface, ssid, **security, passphrase);
        SIHD_UNEXPECTED_RETURN(network);
        added = *network;
        selected = std::move(*network);
    }
    else
        selected = known->front().path;

    auto matched = connection.add_match(dbus::properties_changed_rule(service, interface, interface_interface));
    SIHD_UNEXPECTED_RETURN(matched);
    auto connected = call_with_path(connection, interface, "SelectNetwork", selected);
    if (connected.has_value())
        connected = wait_completed(connection, interface, ssid, timeout);
    if (!connected.has_value() && added.has_value())
        (void)call_with_path(connection, interface, "RemoveNetwork", *added);
    return connected;
}

Result<void> disconnect(dbus::Connection & connection, const std::string & interface)
{
    auto request = dbus::Message::method_call(service, interface, interface_interface, "Disconnect");
    SIHD_UNEXPECTED_RETURN(request);
    auto reply = request->call(connection, call_timeout);
    if (!reply && dbus::has_error_name(reply.error(), "fi.w1.wpa_supplicant1.NotConnected"))
        return {};
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

Result<std::vector<KnownNetwork>> known_networks(dbus::Connection & connection, const std::string & interface)
{
    auto found = networks(connection, interface);
    SIHD_UNEXPECTED_RETURN(found);
    std::vector<KnownNetwork> known;
    for (const NetworkObject & network : *found)
        known.emplace_back(KnownNetwork {.ssid = network.ssid, .security = key_mgmt_security(network.key_mgmt)});
    return known;
}

Result<void> forget(dbus::Connection & connection, const std::string & interface, const std::string & ssid)
{
    auto known = networks_of(connection, interface, ssid);
    SIHD_UNEXPECTED_RETURN(known);
    if (known->empty())
        return std::unexpected(Error(ErrorCode::not_found, "no known network {}", ssid));
    for (const NetworkObject & network : *known)
    {
        auto removed = call_with_path(connection, interface, "RemoveNetwork", network.path);
        SIHD_UNEXPECTED_RETURN(removed);
    }
    return {};
}

std::vector<std::string> monitor_rules(const std::string & interface)
{
    return {dbus::signal_rule(service, interface, interface_interface, "ScanDone"),
            dbus::properties_changed_rule(service, interface, interface_interface)};
}

Result<std::vector<Monitor::Event>> events_of(const dbus::Message & message, const std::string & interface)
{
    std::vector<Monitor::Event> events;
    if (message.path() != interface)
        return events;
    if (is_scan_done(message, interface))
    {
        auto success = dbus::Iter(message).get<bool>();
        SIHD_UNEXPECTED_RETURN(success);
        if (*success)
            events.emplace_back(Monitor::Event {.type = Monitor::Event::Type::scan_done, .state = State::disconnected});
        return events;
    }
    auto state = changed_state(message);
    SIHD_UNEXPECTED_RETURN(state);
    if (state->has_value())
        events.emplace_back(Monitor::Event {.type = Monitor::Event::Type::state_changed, .state = to_state(**state)});
    return events;
}

} // namespace sihd::wifi::wpa_supplicant
