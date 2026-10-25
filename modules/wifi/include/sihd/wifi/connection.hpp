#ifndef __SIHD_WIFI_CONNECTION_HPP__
#define __SIHD_WIFI_CONNECTION_HPP__

#include <string>
#include <vector>

#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/wifi/scan.hpp>

namespace sihd::wifi
{

// the daemon managing an interface: every action goes through it, never around it
enum class Backend
{
    network_manager,
    iwd,
    wpa_supplicant,
    wlan_service
};

enum class State
{
    disconnected,
    connecting,
    connected,
    disconnecting
};

std::string backend_str(Backend backend);
std::string state_str(State state);

struct KnownNetwork
{
        std::string ssid;
        Security security = Security::unknown;
};

// not_found unless a daemon manages the interface
sihd::util::Result<Backend> backend(const std::string & interface_name);
sihd::util::Result<State> state(const std::string & interface_name);

// a known or an open network the backend saw, blocks until connected: a failure leaves the known networks as they were
sihd::util::Result<void>
    connect(const std::string & interface_name, const std::string & ssid, sihd::util::Duration timeout);
// an ssid the backend does not know yet, already_exists otherwise: forget() it first
sihd::util::Result<void> connect(const std::string & interface_name,
                                 const std::string & ssid,
                                 const std::string & passphrase,
                                 sihd::util::Duration timeout);
// never fails on a disconnected interface
sihd::util::Result<void> disconnect(const std::string & interface_name);

sihd::util::Result<std::vector<KnownNetwork>> known_networks(const std::string & interface_name);
sihd::util::Result<void> forget(const std::string & interface_name, const std::string & ssid);

} // namespace sihd::wifi

#endif
