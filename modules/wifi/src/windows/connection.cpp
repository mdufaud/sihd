#include <algorithm>
#include <span>

#include <fmt/format.h>

#include <sihd/util/Defer.hpp>
#include <sihd/util/str.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/scan.hpp>

#include "../connect.hpp"
#include "wlanapi.hpp"

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

// DOT11_AUTH_ALGO_OWE and DOT11_AUTH_ALGO_WPA3_ENT are never declared below NTDDI_WIN10_VB / FE targets
constexpr uint32_t auth_algo_owe = 10;
constexpr uint32_t auth_algo_wpa3_enterprise = 11;
constexpr std::string_view profile_namespace = "http:"
                                               "//www.microsoft.com/networking/WLAN/profile/v1";

struct Network
{
        std::string profile;
        DOT11_AUTH_ALGORITHM algorithm = DOT11_AUTH_ALGO_80211_OPEN;
        bool security_enabled = false;
};

struct Profile
{
        std::string name;
        std::string ssid;
        Security security = Security::unknown;
};

std::string ssid_of(const DOT11_SSID & ssid)
{
    return std::string(ssid.ucSSID, ssid.ucSSID + ssid.uSSIDLength);
}

std::string first_element_text(const std::string & xml, const std::string & tag, size_t offset = 0)
{
    const std::string opening = fmt::format("<{}>", tag);
    const size_t start = xml.find(opening, offset);
    if (start == std::string::npos)
        return {};
    const size_t text = start + opening.size();
    const size_t end = xml.find(fmt::format("</{}>", tag), text);
    return end == std::string::npos ? std::string() : xml.substr(text, end - text);
}

Security profile_security(const std::string & xml)
{
    const std::string authentication = first_element_text(xml, "authentication");
    if (authentication == "open")
        return first_element_text(xml, "encryption") == "WEP" ? Security::wep : Security::open;
    if (authentication == "WPAPSK" || authentication == "WPA2PSK" || authentication == "WPA3SAE")
        return Security::personal;
    if (authentication == "OWE")
        return Security::owe;
    if (authentication.starts_with("WPA"))
        return Security::enterprise;
    return Security::unknown;
}

Result<std::optional<Network>> find_network(wlanapi::Session & session, const GUID & guid, const std::string & ssid)
{
    PWLAN_AVAILABLE_NETWORK_LIST list = nullptr;
    if (const DWORD code = session.api().get_available_network_list(session.client(), &guid, 0, nullptr, &list);
        code != ERROR_SUCCESS)
        return std::unexpected(wlanapi::error(code, "wlan networks"));
    Defer free_list([&session, list] { session.api().free_memory(list); });

    std::optional<Network> found;
    for (const WLAN_AVAILABLE_NETWORK & entry : std::span(list->Network, list->dwNumberOfItems))
    {
        if (ssid_of(entry.dot11Ssid) != ssid || entry.dot11BssType != dot11_BSS_type_infrastructure)
            continue;
        const bool profiled = (entry.dwFlags & WLAN_AVAILABLE_NETWORK_HAS_PROFILE) != 0;
        if (found.has_value() && (!found->profile.empty() || !profiled))
            continue;
        found = Network {
            .profile = profiled ? wlanapi::narrow(entry.strProfileName) : std::string(),
            .algorithm = entry.dot11DefaultAuthAlgorithm,
            .security_enabled = entry.bSecurityEnabled != FALSE,
        };
    }
    return found;
}

Security to_security(const Network & network)
{
    switch (static_cast<uint32_t>(network.algorithm))
    {
        case DOT11_AUTH_ALGO_80211_OPEN:
            return network.security_enabled ? Security::wep : Security::open;
        case DOT11_AUTH_ALGO_80211_SHARED_KEY:
            return Security::wep;
        case DOT11_AUTH_ALGO_WPA_PSK:
        case DOT11_AUTH_ALGO_RSNA_PSK:
        case DOT11_AUTH_ALGO_WPA3_SAE:
            return Security::personal;
        case DOT11_AUTH_ALGO_WPA:
        case DOT11_AUTH_ALGO_RSNA:
        case DOT11_AUTH_ALGO_WPA3:
        case auth_algo_wpa3_enterprise:
            return Security::enterprise;
        case auth_algo_owe:
            return Security::owe;
        default:
            return Security::unknown;
    }
}

// only for what check_target() lets through
Result<std::string> authentication_of(const Network & network, const std::string & ssid)
{
    switch (static_cast<uint32_t>(network.algorithm))
    {
        case DOT11_AUTH_ALGO_80211_OPEN:
            return std::string("open");
        case DOT11_AUTH_ALGO_WPA_PSK:
            return std::string("WPAPSK");
        case DOT11_AUTH_ALGO_RSNA_PSK:
            return std::string("WPA2PSK");
        case DOT11_AUTH_ALGO_WPA3_SAE:
            return std::string("WPA3SAE");
        case auth_algo_owe:
            return std::string("OWE");
        default:
            return std::unexpected(
                Error(ErrorCode::not_supported, "{} needs an authentication that is not supported", ssid));
    }
}

std::string profile_xml(const std::string & ssid,
                        const std::string & authentication,
                        const std::optional<std::string> & passphrase)
{
    const std::string encryption = authentication == "open" ? "none" : "AES";
    std::string shared_key;
    if (passphrase.has_value())
    {
        shared_key = fmt::format("<sharedKey><keyType>passPhrase</keyType><protected>false</protected>"
                                 "<keyMaterial>{}</keyMaterial></sharedKey>",
                                 str::escape_xml(*passphrase));
    }
    return fmt::format(R"(<?xml version="1.0"?><WLANProfile xmlns="{5}">)"
                       "<name>{0}</name><SSIDConfig><SSID><hex>{1}</hex><name>{0}</name></SSID></SSIDConfig>"
                       "<connectionType>ESS</connectionType><connectionMode>auto</connectionMode>"
                       "<MSM><security><authEncryption><authentication>{2}</authentication>"
                       "<encryption>{3}</encryption><useOneX>false</useOneX></authEncryption>{4}</security></MSM>"
                       "</WLANProfile>",
                       str::escape_xml(ssid),
                       str::hexdump(ssid.data(), ssid.size(), '\0'),
                       authentication,
                       encryption,
                       shared_key,
                       profile_namespace);
}

// already_exists when a profile has that name: it is never overwritten
Result<void>
    add_profile(wlanapi::Session & session, const GUID & guid, const std::string & ssid, const std::string & xml)
{
    DWORD reason = 0;
    if (const DWORD code = session.api().set_profile(session.client(),
                                                     &guid,
                                                     0,
                                                     wlanapi::wide(xml).c_str(),
                                                     nullptr,
                                                     FALSE,
                                                     nullptr,
                                                     &reason);
        code != ERROR_SUCCESS)
        return std::unexpected(wlanapi::error(code, fmt::format("wlan profile of {} (reason {})", ssid, reason)));
    return {};
}

Result<void> connect_profile(wlanapi::Session & session,
                             const GUID & guid,
                             const std::string & profile,
                             const std::string & ssid,
                             Duration timeout)
{
    auto listening = session.listen();
    SIHD_UNEXPECTED_RETURN(listening);
    const std::wstring profile_name = wlanapi::wide(profile);
    WLAN_CONNECTION_PARAMETERS parameters {};
    parameters.wlanConnectionMode = wlan_connection_mode_profile;
    parameters.strProfile = profile_name.c_str();
    parameters.dot11BssType = dot11_BSS_type_infrastructure;
    if (const DWORD code = session.api().connect(session.client(), &guid, &parameters, nullptr); code != ERROR_SUCCESS)
        return std::unexpected(wlanapi::error(code, fmt::format("connecting to {}", ssid)));

    const std::optional<wlanapi::Notification> outcome = session.wait(
        guid,
        {wlan_notification_acm_connection_complete, wlan_notification_acm_connection_attempt_fail},
        timeout);
    if (!outcome.has_value())
        return std::unexpected(Error(ErrorCode::timeout, "wlan notification timed out: connecting to {}", ssid));
    if (outcome->code == wlan_notification_acm_connection_attempt_fail || outcome->reason != WLAN_REASON_CODE_SUCCESS)
        return std::unexpected(
            Error(ErrorCode::io_error, "the wlan service failed: reason {}: connecting to {}", outcome->reason, ssid));
    return {};
}

Result<void> delete_profile(wlanapi::Session & session, const GUID & guid, const std::string & name)
{
    if (const DWORD code = session.api().delete_profile(session.client(), &guid, wlanapi::wide(name).c_str(), nullptr);
        code != ERROR_SUCCESS)
        return std::unexpected(wlanapi::error(code, fmt::format("wlan profile {}", name)));
    return {};
}

Result<std::vector<Profile>> profiles_of(wlanapi::Session & session, const GUID & guid)
{
    PWLAN_PROFILE_INFO_LIST list = nullptr;
    if (const DWORD code = session.api().get_profile_list(session.client(), &guid, nullptr, &list);
        code != ERROR_SUCCESS)
        return std::unexpected(wlanapi::error(code, "wlan profiles"));
    Defer free_list([&session, list] { session.api().free_memory(list); });

    std::vector<Profile> profiles;
    for (const WLAN_PROFILE_INFO & info : std::span(list->ProfileInfo, list->dwNumberOfItems))
    {
        LPWSTR xml = nullptr;
        DWORD flags = 0;
        DWORD access = 0;
        if (const DWORD code = session.api().get_profile(session.client(),
                                                         &guid,
                                                         info.strProfileName,
                                                         nullptr,
                                                         &xml,
                                                         &flags,
                                                         &access);
            code != ERROR_SUCCESS)
            return std::unexpected(wlanapi::error(code, "wlan profile"));
        Defer free_xml([&session, xml] { session.api().free_memory(xml); });

        const std::string text = wlanapi::narrow(xml);
        const size_t ssid_config = text.find("<SSID>");
        const size_t offset = ssid_config == std::string::npos ? 0 : ssid_config;
        auto bytes = str::from_hex(first_element_text(text, "hex", offset));
        const bool hex_ssid = bytes.has_value() && !bytes->empty();
        profiles.emplace_back(Profile {
            .name = wlanapi::narrow(info.strProfileName),
            .ssid = hex_ssid ? std::string(bytes->begin(), bytes->end()) : first_element_text(text, "name", offset),
            .security = profile_security(text),
        });
    }
    return profiles;
}

} // namespace

Result<Backend> backend(const std::string & interface_name)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    return Backend::wlan_service;
}

Result<State> state(const std::string & interface_name)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto interface_state = session.interface_state(*guid);
    SIHD_UNEXPECTED_RETURN(interface_state);
    switch (*interface_state)
    {
        case wlan_interface_state_connected:
            return State::connected;
        case wlan_interface_state_associating:
        case wlan_interface_state_discovering:
        case wlan_interface_state_authenticating:
            return State::connecting;
        case wlan_interface_state_disconnecting:
            return State::disconnecting;
        default:
            return State::disconnected;
    }
}

Result<void> scan(const std::string & interface_name, Duration timeout)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto listening = session.listen();
    SIHD_UNEXPECTED_RETURN(listening);
    if (const DWORD code = session.api().scan(session.client(), &*guid, nullptr, nullptr, nullptr);
        code != ERROR_SUCCESS)
        return std::unexpected(wlanapi::error(code, "wlan scan"));

    const std::optional<wlanapi::Notification> outcome = session.wait(
        *guid,
        {wlan_notification_acm_scan_complete, wlan_notification_acm_scan_fail},
        timeout);
    if (!outcome.has_value())
        return std::unexpected(Error(ErrorCode::timeout, "wlan scan timed out"));
    if (outcome->code == wlan_notification_acm_scan_fail)
        return std::unexpected(Error(ErrorCode::io_error, "wlan scan failed: reason {}", outcome->reason));
    return {};
}

Result<void> connect_backend(const std::string & interface_name,
                             const std::string & ssid,
                             const std::optional<std::string> & passphrase,
                             Duration timeout)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto network = find_network(session, *guid, ssid);
    SIHD_UNEXPECTED_RETURN(network);
    if (!network->has_value())
        return std::unexpected(Error(ErrorCode::not_found, "the wlan service did not see {}", ssid));
    const Target target {.security = to_security(**network), .known = !(*network)->profile.empty()};
    auto acceptable = check_target(ssid, target, passphrase);
    SIHD_UNEXPECTED_RETURN(acceptable);
    if (target.known)
        return connect_profile(session, *guid, (*network)->profile, ssid, timeout);

    auto authentication = authentication_of(**network, ssid);
    SIHD_UNEXPECTED_RETURN(authentication);
    auto added = add_profile(session, *guid, ssid, profile_xml(ssid, *authentication, passphrase));
    SIHD_UNEXPECTED_RETURN(added);
    auto connected = connect_profile(session, *guid, ssid, ssid, timeout);
    if (!connected.has_value())
        (void)delete_profile(session, *guid, ssid);
    return connected;
}

Result<void> disconnect(const std::string & interface_name)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    if (const DWORD code = session.api().disconnect(session.client(), &*guid, nullptr); code != ERROR_SUCCESS)
        return std::unexpected(wlanapi::error(code, "wlan disconnect"));
    return {};
}

Result<std::vector<KnownNetwork>> known_networks(const std::string & interface_name)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto profiles = profiles_of(session, *guid);
    SIHD_UNEXPECTED_RETURN(profiles);
    std::vector<KnownNetwork> known;
    for (const Profile & profile : *profiles)
        known.emplace_back(KnownNetwork {.ssid = profile.ssid, .security = profile.security});
    return known;
}

Result<void> forget(const std::string & interface_name, const std::string & ssid)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto profiles = profiles_of(session, *guid);
    SIHD_UNEXPECTED_RETURN(profiles);
    std::erase_if(*profiles, [&ssid](const Profile & profile) { return profile.ssid != ssid; });
    if (profiles->empty())
        return std::unexpected(Error(ErrorCode::not_found, "no known network {}", ssid));
    for (const Profile & profile : *profiles)
    {
        auto deleted = delete_profile(session, *guid, profile.name);
        SIHD_UNEXPECTED_RETURN_CTX(deleted, "forgetting {}", ssid);
    }
    return {};
}

} // namespace sihd::wifi
