#include <cstring>
#include <span>

#include <sihd/net/ip.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/wifi/scan.hpp>

#include "wlanapi.hpp"

using namespace sihd::util;

namespace sihd::wifi
{

namespace
{

constexpr uint16_t capability_privacy = 0x0010;

AccessPoint read_bss(const WLAN_BSS_ENTRY & entry, const std::optional<WLAN_CONNECTION_ATTRIBUTES> & connection)
{
    // the elements always follow the entry, ulIeOffset bytes from its start
    const ArrUByteView information_elements(reinterpret_cast<const uint8_t *>(&entry) + entry.ulIeOffset,
                                            entry.ulIeSize);
    AccessPoint access_point;
    access_point.ssid.assign(entry.dot11Ssid.ucSSID, entry.dot11Ssid.ucSSID + entry.dot11Ssid.uSSIDLength);
    access_point.bssid = net::ip::mac_str(entry.dot11Bssid);
    access_point.signal_dbm = static_cast<int>(entry.lRssi);
    access_point.frequency_mhz = static_cast<uint32_t>(entry.ulChCenterFrequency / wlanapi::khz_per_mhz);
    access_point.security = security_of(information_elements,
                                        (entry.usCapabilityInformation & capability_privacy) != 0);
    access_point.associated = connection.has_value()
                              && std::memcmp(entry.dot11Bssid,
                                             connection->wlanAssociationAttributes.dot11Bssid,
                                             sizeof(DOT11_MAC_ADDRESS))
                                     == 0;
    return access_point;
}

} // namespace

Result<std::vector<AccessPoint>> access_points(const std::string & interface_name)
{
    wlanapi::Session session;
    auto guid = session.open(interface_name);
    SIHD_UNEXPECTED_RETURN(guid);
    auto connection = session.connection(*guid);
    SIHD_UNEXPECTED_RETURN(connection);

    PWLAN_BSS_LIST bss_list = nullptr;
    if (const DWORD code = session.api().get_network_bss_list(session.client(),
                                                              &*guid,
                                                              nullptr,
                                                              dot11_BSS_type_any,
                                                              FALSE,
                                                              nullptr,
                                                              &bss_list);
        code != ERROR_SUCCESS)
        return std::unexpected(wlanapi::error(code, "wlan bss list"));
    Defer free_bss_list([&session, bss_list] { session.api().free_memory(bss_list); });

    std::vector<AccessPoint> found;
    for (const WLAN_BSS_ENTRY & entry : std::span(bss_list->wlanBssEntries, bss_list->dwNumberOfItems))
        found.emplace_back(read_bss(entry, *connection));
    return found;
}

} // namespace sihd::wifi
