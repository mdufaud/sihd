#include <sihd/util/Error.hpp>
#include <sihd/wifi/connection.hpp>

#include "connect.hpp"

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr size_t ssid_max_size = 32;
constexpr size_t passphrase_min_size = 8;
constexpr size_t passphrase_max_size = 63;

Result<void> check_ssid(const std::string & ssid)
{
    if (ssid.empty() || ssid.size() > ssid_max_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "an ssid holds 1 to 32 bytes"));
    return {};
}

} // namespace

std::string backend_str(Backend backend)
{
    switch (backend)
    {
        case Backend::network_manager:
            return "networkmanager";
        case Backend::iwd:
            return "iwd";
        case Backend::wpa_supplicant:
            return "wpa_supplicant";
        case Backend::wlan_service:
            return "wlan service";
    }
    return "unknown";
}

std::string state_str(State state)
{
    switch (state)
    {
        case State::disconnected:
            return "disconnected";
        case State::connecting:
            return "connecting";
        case State::connected:
            return "connected";
        case State::disconnecting:
            return "disconnecting";
    }
    return "unknown";
}

Result<void>
    check_target(const std::string & ssid, const Target & target, const std::optional<std::string> & passphrase)
{
    if (target.known)
    {
        if (passphrase.has_value())
            return std::unexpected(Error(ErrorCode::already_exists, "{} is already known: forget it first", ssid));
        return {};
    }
    switch (target.security)
    {
        case Security::wep:
            return std::unexpected(Error(ErrorCode::not_supported, "{} needs wep, which is not supported", ssid));
        case Security::enterprise:
            return std::unexpected(Error(ErrorCode::not_supported, "{} needs 802.1x, which is not supported", ssid));
        case Security::open:
        case Security::owe:
            if (passphrase.has_value())
                return std::unexpected(Error(ErrorCode::invalid_argument, "{} takes no passphrase", ssid));
            return {};
        case Security::personal:
            if (!passphrase.has_value())
                return std::unexpected(Error(ErrorCode::invalid_argument, "{} needs a passphrase", ssid));
            return {};
        case Security::unknown:
            return {};
    }
    return {};
}

Result<void> connect(const std::string & interface_name, const std::string & ssid, Duration timeout)
{
    auto valid = check_ssid(ssid);
    SIHD_UNEXPECTED_RETURN(valid);
    return connect_backend(interface_name, ssid, std::nullopt, timeout);
}

Result<void> connect(const std::string & interface_name,
                     const std::string & ssid,
                     const std::string & passphrase,
                     Duration timeout)
{
    auto valid = check_ssid(ssid);
    SIHD_UNEXPECTED_RETURN(valid);
    if (passphrase.size() < passphrase_min_size || passphrase.size() > passphrase_max_size)
        return std::unexpected(Error(ErrorCode::invalid_argument, "a passphrase holds 8 to 63 characters"));
    return connect_backend(interface_name, ssid, passphrase, timeout);
}

} // namespace sihd::wifi
