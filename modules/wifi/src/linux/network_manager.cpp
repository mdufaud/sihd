#include <algorithm>

#include <sihd/util/Error.hpp>

#include "../connect.hpp"
#include "backends.hpp"

using namespace sihd::util;

namespace sihd::wifi::network_manager
{

namespace
{

constexpr ZStringView service = "org.freedesktop.NetworkManager";
constexpr ZStringView root_path = "/org/freedesktop/NetworkManager";
constexpr ZStringView settings_path = "/org/freedesktop/NetworkManager/Settings";
constexpr ZStringView root_interface = "org.freedesktop.NetworkManager";
constexpr ZStringView device_interface = "org.freedesktop.NetworkManager.Device";
constexpr ZStringView wireless_interface = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr ZStringView access_point_interface = "org.freedesktop.NetworkManager.AccessPoint";
constexpr ZStringView settings_interface = "org.freedesktop.NetworkManager.Settings";
constexpr ZStringView profile_interface = "org.freedesktop.NetworkManager.Settings.Connection";
constexpr ZStringView wireless_setting = "802-11-wireless";
constexpr ZStringView security_setting = "802-11-wireless-security";

constexpr uint32_t device_type_wifi = 2;
constexpr uint32_t state_prepare = 40;
constexpr uint32_t state_activated = 100;
constexpr uint32_t state_deactivating = 110;
constexpr uint32_t state_failed = 120;

constexpr uint32_t ap_flag_privacy = 0x1;
constexpr uint32_t key_mgmt_psk = 0x100;
constexpr uint32_t key_mgmt_8021x = 0x200;
constexpr uint32_t key_mgmt_sae = 0x400;
constexpr uint32_t key_mgmt_owe = 0x800;
constexpr uint32_t key_mgmt_owe_transition = 0x1000;
constexpr uint32_t key_mgmt_suite_b_192 = 0x2000;

struct Profile
{
        std::string path;
        std::string ssid;
        Security security = Security::open;
        bool wifi = false;
};

struct AccessPointObject
{
        std::string path;
        std::string ssid;
        uint8_t strength = 0;
        uint32_t flags = 0;
        uint32_t wpa_rsn_flags = 0;
};

State to_state(uint32_t device_state)
{
    if (device_state == state_activated)
        return State::connected;
    if (device_state == state_deactivating)
        return State::disconnecting;
    if (device_state >= state_prepare && device_state < state_activated)
        return State::connecting;
    return State::disconnected;
}

Security key_mgmt_security(const std::string & key_mgmt)
{
    if (key_mgmt == "wpa-psk" || key_mgmt == "sae")
        return Security::personal;
    if (key_mgmt == "wpa-eap" || key_mgmt == "wpa-eap-suite-b-192")
        return Security::enterprise;
    if (key_mgmt == "owe")
        return Security::owe;
    if (key_mgmt == "none" || key_mgmt == "ieee8021x")
        return Security::wep;
    return Security::unknown;
}

// same precedence as security_of() over the information elements
Security access_point_security(const AccessPointObject & access_point)
{
    if ((access_point.wpa_rsn_flags & (key_mgmt_psk | key_mgmt_sae)) != 0)
        return Security::personal;
    if ((access_point.wpa_rsn_flags & (key_mgmt_8021x | key_mgmt_suite_b_192)) != 0)
        return Security::enterprise;
    if ((access_point.wpa_rsn_flags & (key_mgmt_owe | key_mgmt_owe_transition)) != 0)
        return Security::owe;
    return (access_point.flags & ap_flag_privacy) != 0 ? Security::wep : Security::open;
}

Result<dbus::Message> call(dbus::Connection & connection, ZStringView path, ZStringView interface, ZStringView member)
{
    auto request = dbus::Message::method_call(service, path, interface, member);
    SIHD_UNEXPECTED_RETURN(request);
    return request->call(connection, call_timeout);
}

Result<void>
    read_profile_value(Profile & profile, const std::string & setting, const std::string & key, dbus::Iter & value)
{
    if (setting == "connection" && key == "type")
    {
        auto type = value.get<std::string>();
        SIHD_UNEXPECTED_RETURN(type);
        profile.wifi = *type == wireless_setting;
    }
    else if (setting == wireless_setting && key == "ssid")
    {
        auto ssid = value.get_array<uint8_t>();
        SIHD_UNEXPECTED_RETURN(ssid);
        profile.ssid.assign(ssid->begin(), ssid->end());
    }
    else if (setting == security_setting && key == "key-mgmt")
    {
        auto key_mgmt = value.get<std::string>();
        SIHD_UNEXPECTED_RETURN(key_mgmt);
        profile.security = key_mgmt_security(*key_mgmt);
    }
    return {};
}

Result<std::vector<Profile>> wifi_profiles(dbus::Connection & connection)
{
    auto reply = call(connection, settings_path, settings_interface, "ListConnections");
    SIHD_UNEXPECTED_RETURN(reply);
    auto paths = dbus::Iter(*reply).get_array<std::string>();
    SIHD_UNEXPECTED_RETURN(paths);

    std::vector<dbus::Message> requests;
    for (const std::string & path : *paths)
    {
        auto request = dbus::Message::method_call(service, path, profile_interface, "GetSettings");
        SIHD_UNEXPECTED_RETURN(request);
        requests.emplace_back(std::move(*request));
    }
    auto settings = dbus::Message::call_all(connection, requests, call_timeout);
    SIHD_UNEXPECTED_RETURN(settings);

    std::vector<Profile> profiles;
    for (size_t index = 0; index < paths->size(); ++index)
    {
        Profile profile;
        profile.path = (*paths)[index];
        auto parsed = dbus::Iter((*settings)[index])
                          .for_each_entry<std::string>([&profile](const std::string & setting, dbus::Iter & values) {
                              return values.for_each_entry<std::string>(
                                  [&profile, &setting](const std::string & key, dbus::Iter & value) {
                                      return read_profile_value(profile, setting, key, value);
                                  });
                          });
        SIHD_UNEXPECTED_RETURN(parsed);
        if (profile.wifi)
            profiles.emplace_back(std::move(profile));
    }
    return profiles;
}

Result<std::vector<Profile>> profiles_of(dbus::Connection & connection, const std::string & ssid)
{
    auto profiles = wifi_profiles(connection);
    SIHD_UNEXPECTED_RETURN(profiles);
    std::erase_if(*profiles, [&ssid](const Profile & profile) { return profile.ssid != ssid; });
    return profiles;
}

Result<void> delete_profile(dbus::Connection & connection, const std::string & path)
{
    auto reply = call(connection, path, profile_interface, "Delete");
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

Result<void> read_access_point_value(AccessPointObject & access_point, const std::string & name, dbus::Iter & value)
{
    if (name == "Ssid")
    {
        auto bytes = value.get_array<uint8_t>();
        SIHD_UNEXPECTED_RETURN(bytes);
        access_point.ssid.assign(bytes->begin(), bytes->end());
    }
    else if (name == "Strength")
    {
        auto strength = value.get<uint8_t>();
        SIHD_UNEXPECTED_RETURN(strength);
        access_point.strength = *strength;
    }
    else if (name == "Flags" || name == "WpaFlags" || name == "RsnFlags")
    {
        auto flags = value.get<uint32_t>();
        SIHD_UNEXPECTED_RETURN(flags);
        if (name == "Flags")
            access_point.flags = *flags;
        else
            access_point.wpa_rsn_flags |= *flags;
    }

    return {};
}

Result<std::optional<AccessPointObject>>
    strongest_access_point(dbus::Connection & connection, const std::string & device, const std::string & ssid)
{
    auto reply = call(connection, device, wireless_interface, "GetAllAccessPoints");
    SIHD_UNEXPECTED_RETURN(reply);
    auto paths = dbus::Iter(*reply).get_array<std::string>();
    SIHD_UNEXPECTED_RETURN(paths);

    std::vector<dbus::Message> requests;
    for (const std::string & path : *paths)
    {
        auto request = dbus::Message::properties_get_all(service, path, access_point_interface);
        SIHD_UNEXPECTED_RETURN(request);
        requests.emplace_back(std::move(*request));
    }
    auto properties = dbus::Message::call_all(connection, requests, call_timeout);
    SIHD_UNEXPECTED_RETURN(properties);

    std::optional<AccessPointObject> strongest;
    for (size_t index = 0; index < paths->size(); ++index)
    {
        AccessPointObject access_point;
        access_point.path = (*paths)[index];
        auto read = dbus::Iter((*properties)[index])
                        .for_each_entry<std::string>([&access_point](const std::string & name, dbus::Iter & value) {
                            return read_access_point_value(access_point, name, value);
                        });
        SIHD_UNEXPECTED_RETURN(read);
        if (access_point.ssid == ssid && (!strongest.has_value() || access_point.strength > strongest->strength))
            strongest = std::move(access_point);
    }
    return strongest;
}

Result<void>
    add_setting(dbus::Appender & settings, ZStringView name, const std::function<Result<void>(dbus::Appender &)> & fill)
{
    auto entry = settings.open_dict_entry();
    SIHD_UNEXPECTED_RETURN(entry);
    auto name_added = entry->add(name);
    SIHD_UNEXPECTED_RETURN(name_added);
    auto values = entry->open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(values);
    auto filled = fill(*values);
    SIHD_UNEXPECTED_RETURN(filled);
    auto values_closed = entry->close(std::move(*values));
    SIHD_UNEXPECTED_RETURN(values_closed);
    return settings.close(std::move(*entry));
}

// never sets key-mgmt: networkmanager completes the profile from the access point
Result<void>
    add_profile(dbus::Message & request, const std::string & ssid, const std::optional<std::string> & passphrase)
{
    dbus::Appender body(request);
    auto settings = body.open_array("{sa{sv}}");
    SIHD_UNEXPECTED_RETURN(settings);
    auto wireless = add_setting(*settings, wireless_setting, [&ssid](dbus::Appender & values) {
        return values.add_entry("ssid", "ay", [&ssid](dbus::Appender & value) {
            return value.add_array(std::vector<uint8_t>(ssid.begin(), ssid.end()));
        });
    });
    SIHD_UNEXPECTED_RETURN(wireless);

    if (passphrase.has_value())
    {
        auto security = add_setting(*settings, security_setting, [&passphrase](dbus::Appender & values) {
            return values.add_entry("psk", "s", [&passphrase](dbus::Appender & value) {
                return value.add(*passphrase);
            });
        });
        SIHD_UNEXPECTED_RETURN(security);
    }
    return body.close(std::move(*settings));
}

Result<void> activate_profile(dbus::Connection & connection,
                              const std::string & profile,
                              const std::string & device,
                              const std::string & access_point)
{
    auto request = dbus::Message::method_call(service, root_path, root_interface, "ActivateConnection");
    SIHD_UNEXPECTED_RETURN(request);
    auto profile_added = request->add_object_path(profile);
    SIHD_UNEXPECTED_RETURN(profile_added);
    auto device_added = request->add_object_path(device);
    SIHD_UNEXPECTED_RETURN(device_added);
    auto access_point_added = request->add_object_path(access_point);
    SIHD_UNEXPECTED_RETURN(access_point_added);
    auto reply = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

// the path of the profile networkmanager created
Result<std::string> add_and_activate(dbus::Connection & connection,
                                     const std::string & device,
                                     const std::string & access_point,
                                     const std::string & ssid,
                                     const std::optional<std::string> & passphrase)
{
    auto request = dbus::Message::method_call(service, root_path, root_interface, "AddAndActivateConnection");
    SIHD_UNEXPECTED_RETURN(request);
    auto described = add_profile(*request, ssid, passphrase);
    SIHD_UNEXPECTED_RETURN(described);
    auto device_added = request->add_object_path(device);
    SIHD_UNEXPECTED_RETURN(device_added);
    auto access_point_added = request->add_object_path(access_point);
    SIHD_UNEXPECTED_RETURN(access_point_added);
    auto reply = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(reply);
    return dbus::Iter(*reply).get<std::string>();
}

Result<void> wait_activated(dbus::Connection & connection,
                            const std::string & device,
                            const std::string & ssid,
                            Duration timeout)
{
    auto activated = dbus::Message::receive_until(
        connection,
        timeout,
        [&device](const dbus::Message & message) -> Result<bool> {
            if (message.type() != dbus::Message::Type::signal || message.path() != device
                || message.interface() != device_interface || message.member() != "StateChanged")
                return false;
            dbus::Iter body(message);
            auto new_state = body.get<uint32_t>();
            SIHD_UNEXPECTED_RETURN(new_state);
            body.next();
            body.next();
            auto reason = body.get<uint32_t>();
            SIHD_UNEXPECTED_RETURN(reason);
            if (*new_state == state_failed)
                return std::unexpected(Error(ErrorCode::io_error, "networkmanager failed: reason {}", *reason));
            return *new_state == state_activated;
        });
    SIHD_UNEXPECTED_RETURN_CTX(activated, "connecting to {}", ssid);
    return {};
}

} // namespace

Result<std::optional<std::string>> detect(dbus::Connection & connection, const std::string & interface_name)
{
    auto running = connection.name_has_owner(service);
    SIHD_UNEXPECTED_RETURN(running);
    if (!*running)
        return std::nullopt;

    auto request = dbus::Message::method_call(service, root_path, root_interface, "GetDeviceByIpIface");
    SIHD_UNEXPECTED_RETURN(request);
    auto name_added = request->add(interface_name);
    SIHD_UNEXPECTED_RETURN(name_added);
    auto reply = request->call(connection, call_timeout);
    if (!reply && dbus::has_error_name(reply.error(), "org.freedesktop.NetworkManager.UnknownDevice"))
        return std::nullopt;
    SIHD_UNEXPECTED_RETURN(reply);
    auto device = dbus::Iter(*reply).get<std::string>();
    SIHD_UNEXPECTED_RETURN(device);

    auto managed = dbus::get_property<bool>(connection, service, *device, device_interface, "Managed", call_timeout);
    SIHD_UNEXPECTED_RETURN(managed);
    auto type = dbus::get_property<uint32_t>(connection,
                                             service,
                                             *device,
                                             device_interface,
                                             "DeviceType",
                                             call_timeout);
    SIHD_UNEXPECTED_RETURN(type);
    if (!*managed || *type != device_type_wifi)
        return std::nullopt;
    return *device;
}

Result<State> state(dbus::Connection & connection, const std::string & device)
{
    auto device_state = dbus::get_property<uint32_t>(connection,
                                                     service,
                                                     device,
                                                     device_interface,
                                                     "State",
                                                     call_timeout);
    SIHD_UNEXPECTED_RETURN(device_state);
    return to_state(*device_state);
}

Result<void> scan(dbus::Connection & connection, const std::string & device, Duration timeout)
{
    auto matched = connection.add_match(dbus::properties_changed_rule(service, device, wireless_interface));
    SIHD_UNEXPECTED_RETURN(matched);
    auto request = dbus::Message::method_call(service, device, wireless_interface, "RequestScan");
    SIHD_UNEXPECTED_RETURN(request);
    dbus::Appender body(*request);
    auto options = body.open_array("{sv}");
    SIHD_UNEXPECTED_RETURN(options);
    auto options_closed = body.close(std::move(*options));
    SIHD_UNEXPECTED_RETURN(options_closed);
    auto requested = request->call(connection, call_timeout);
    SIHD_UNEXPECTED_RETURN(requested);

    auto scanned = dbus::Message::receive_until(
        connection,
        timeout,
        [&device](const dbus::Message & message) -> Result<bool> {
            if (message.path() != device)
                return false;
            auto last_scan = message.changed_property(wireless_interface, "LastScan");
            SIHD_UNEXPECTED_RETURN(last_scan);
            return last_scan->has_value();
        });
    SIHD_UNEXPECTED_RETURN_CTX(scanned, "networkmanager scan");
    return {};
}

Result<void> connect(dbus::Connection & connection,
                     const std::string & device,
                     const std::string & ssid,
                     const std::optional<std::string> & passphrase,
                     Duration timeout)
{
    auto access_point = strongest_access_point(connection, device, ssid);
    SIHD_UNEXPECTED_RETURN(access_point);
    if (!access_point->has_value())
        return std::unexpected(Error(ErrorCode::not_found, "networkmanager did not see {}", ssid));
    auto known = profiles_of(connection, ssid);
    SIHD_UNEXPECTED_RETURN(known);
    const Target target {.security = access_point_security(**access_point), .known = !known->empty()};
    auto acceptable = check_target(ssid, target, passphrase);
    SIHD_UNEXPECTED_RETURN(acceptable);

    auto matched = connection.add_match(dbus::signal_rule(service, device, device_interface, "StateChanged"));
    SIHD_UNEXPECTED_RETURN(matched);
    if (target.known)
    {
        auto activated = activate_profile(connection, known->front().path, device, (*access_point)->path);
        SIHD_UNEXPECTED_RETURN(activated);
        return wait_activated(connection, device, ssid, timeout);
    }

    auto created = add_and_activate(connection, device, (*access_point)->path, ssid, passphrase);
    SIHD_UNEXPECTED_RETURN(created);
    auto connected = wait_activated(connection, device, ssid, timeout);
    if (!connected.has_value())
        (void)delete_profile(connection, *created);
    return connected;
}

Result<void> disconnect(dbus::Connection & connection, const std::string & device)
{
    auto reply = call(connection, device, device_interface, "Disconnect");
    if (!reply && dbus::has_error_name(reply.error(), "org.freedesktop.NetworkManager.Device.NotActive"))
        return {};
    SIHD_UNEXPECTED_RETURN(reply);
    return {};
}

Result<std::vector<KnownNetwork>> known_networks(dbus::Connection & connection, const std::string &)
{
    auto profiles = wifi_profiles(connection);
    SIHD_UNEXPECTED_RETURN(profiles);
    std::vector<KnownNetwork> known;
    for (const Profile & profile : *profiles)
        known.emplace_back(KnownNetwork {.ssid = profile.ssid, .security = profile.security});
    return known;
}

Result<void> forget(dbus::Connection & connection, const std::string &, const std::string & ssid)
{
    auto known = profiles_of(connection, ssid);
    SIHD_UNEXPECTED_RETURN(known);
    if (known->empty())
        return std::unexpected(Error(ErrorCode::not_found, "no known network {}", ssid));
    for (const Profile & profile : *known)
    {
        auto deleted = delete_profile(connection, profile.path);
        SIHD_UNEXPECTED_RETURN(deleted);
    }
    return {};
}

std::vector<std::string> monitor_rules(const std::string & device)
{
    return {dbus::signal_rule(service, device, device_interface, "StateChanged"),
            dbus::properties_changed_rule(service, device, wireless_interface)};
}

Result<std::vector<Monitor::Event>> events_of(const dbus::Message & message, const std::string & device)
{
    std::vector<Monitor::Event> events;
    if (message.type() != dbus::Message::Type::signal || message.path() != device)
        return events;
    if (message.interface() == device_interface && message.member() == "StateChanged")
    {
        auto new_state = dbus::Iter(message).get<uint32_t>();
        SIHD_UNEXPECTED_RETURN(new_state);
        events.emplace_back(
            Monitor::Event {.type = Monitor::Event::Type::state_changed, .state = to_state(*new_state)});
        return events;
    }
    auto last_scan = message.changed_property(wireless_interface, "LastScan");
    SIHD_UNEXPECTED_RETURN(last_scan);
    if (last_scan->has_value())
        events.emplace_back(Monitor::Event {.type = Monitor::Event::Type::scan_done, .state = State::disconnected});
    return events;
}

} // namespace sihd::wifi::network_manager
