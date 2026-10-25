#ifndef __SIHD_WIFI_SCAN_HPP__
#define __SIHD_WIFI_SCAN_HPP__

#include <cstdint>
#include <string>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::wifi
{

enum class Security
{
    unknown,
    open,
    owe,
    wep,
    // psk or sae: a passphrase is required
    personal,
    enterprise
};

std::string security_str(Security security);

struct AccessPoint
{
        std::string ssid;
        std::string bssid;
        int signal_dbm = 0;
        uint32_t frequency_mhz = 0;
        Security security = Security::unknown;
        sihd::util::Duration seen_ago;
        bool associated = false;
};

// never triggers a scan: what the radio heard lately, linux keeps the last 30 seconds of it
sihd::util::Result<std::vector<AccessPoint>> access_points(const std::string & interface_name);
// asks the backend for a scan, blocks until access_points() holds its results
sihd::util::Result<void> scan(const std::string & interface_name, sihd::util::Duration timeout);

// elements of a beacon or probe response, privacy must be the capability bit
Security security_of(sihd::util::ArrUByteView information_elements, bool privacy);

} // namespace sihd::wifi

#endif
