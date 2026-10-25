#include <algorithm>
#include <cstring>
#include <span>

#include <sihd/net/ip.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/wifi/link.hpp>

#include "wlanapi.hpp"

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

// wlanSignalQuality 0..100 always stands for -100..-50 dBm, linear in between
constexpr int quality_floor_dbm = -100;
constexpr int quality_points_per_dbm = 2;
constexpr uint64_t bps_per_kbps = 1000;

void read_bss(wlanapi::Session & session,
              const GUID & guid,
              const WLAN_CONNECTION_ATTRIBUTES & connection,
              Link & result)
{
    DOT11_SSID ssid = connection.wlanAssociationAttributes.dot11Ssid;
    PWLAN_BSS_LIST bss_list = nullptr;
    if (session.api().get_network_bss_list(session.client(),
                                           &guid,
                                           &ssid,
                                           dot11_BSS_type_infrastructure,
                                           connection.wlanSecurityAttributes.bSecurityEnabled,
                                           nullptr,
                                           &bss_list)
        != ERROR_SUCCESS)
        return;
    Defer free_bss_list([&session, bss_list] { session.api().free_memory(bss_list); });

    const std::span<const WLAN_BSS_ENTRY> entries(bss_list->wlanBssEntries, bss_list->dwNumberOfItems);
    const auto entry = std::ranges::find_if(entries, [&connection](const WLAN_BSS_ENTRY & candidate) {
        return std::memcmp(candidate.dot11Bssid,
                           connection.wlanAssociationAttributes.dot11Bssid,
                           sizeof(DOT11_MAC_ADDRESS))
               == 0;
    });
    if (entry == entries.end())
        return;
    result.signal_dbm = static_cast<int>(entry->lRssi);
    result.frequency_mhz = static_cast<uint32_t>(entry->ulChCenterFrequency / wlanapi::khz_per_mhz);
}

} // namespace

Result<Link> link(const std::string & interface_name)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto connection = session.connection(*guid);
    SIHD_UNEXPECTED_RETURN(connection);
    if (!connection->has_value())
        return std::unexpected(Error(ErrorCode::not_found, "{} is not an associated wifi station", interface_name));

    const WLAN_ASSOCIATION_ATTRIBUTES & association = (*connection)->wlanAssociationAttributes;
    Link result;
    result.ssid.assign(association.dot11Ssid.ucSSID, association.dot11Ssid.ucSSID + association.dot11Ssid.uSSIDLength);
    result.bssid = net::ip::mac_str(association.dot11Bssid);
    result.tx_bitrate_bps = static_cast<uint64_t>(association.ulTxRate) * bps_per_kbps;
    result.signal_dbm = quality_floor_dbm + static_cast<int>(association.wlanSignalQuality) / quality_points_per_dbm;
    read_bss(session, *guid, **connection, result);
    return result;
}

} // namespace sihd::wifi
