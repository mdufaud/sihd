#ifndef __SIHD_NET_WIFI_HPP__
#define __SIHD_NET_WIFI_HPP__

#include <cstdint>
#include <string>

#include <sihd/util/Result.hpp>

namespace sihd::net::wifi
{

struct Link
{
        std::string ssid;
        // access point MAC, formatted like NetInterface::mac_addr
        std::string bssid;
        // numeric fields are 0 when unknown, windows estimates signal_dbm from the link quality
        int signal_dbm = 0;
        uint32_t frequency_mhz = 0;
        uint64_t tx_bitrate_bps = 0;
};

// not_found unless an associated wifi station
sihd::util::Result<Link> link(const std::string & interface_name);

} // namespace sihd::net::wifi

#endif
