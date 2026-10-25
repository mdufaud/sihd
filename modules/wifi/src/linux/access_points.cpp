#include <sys/socket.h>

#include <sihd/net/ip.hpp>
#include <sihd/util/time.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/scan.hpp>

#include "nl80211.hpp"
#include <linux/genetlink.h>
#include <linux/nl80211.h>

using namespace sihd::util;
using namespace sihd::net::linux_;

namespace sihd::wifi
{

namespace
{

constexpr uint16_t capability_privacy = 0x0010;
constexpr int32_t mbm_per_dbm = 100;

AccessPoint read_bss(const netlink::Attribute & bss)
{
    AccessPoint access_point;
    ArrUByteView information_elements;
    bool privacy = false;
    for (const netlink::Attribute & attribute : netlink::nested(bss))
    {
        switch (attribute.type)
        {
            case NL80211_BSS_BSSID:
                access_point.bssid = net::ip::mac_str(attribute.data);
                break;
            case NL80211_BSS_FREQUENCY:
                access_point.frequency_mhz = netlink::u32(attribute);
                break;
            case NL80211_BSS_SIGNAL_MBM:
                access_point.signal_dbm = static_cast<int32_t>(netlink::u32(attribute)) / mbm_per_dbm;
                break;
            case NL80211_BSS_SEEN_MS_AGO:
                access_point.seen_ago = Duration(time::milli(netlink::u32(attribute)));
                break;
            case NL80211_BSS_STATUS:
                access_point.associated = netlink::u32(attribute) == NL80211_BSS_STATUS_ASSOCIATED;
                break;
            case NL80211_BSS_CAPABILITY:
                privacy = (netlink::u16(attribute) & capability_privacy) != 0;
                break;
            case NL80211_BSS_INFORMATION_ELEMENTS:
                information_elements = ArrUByteView(attribute.data.data(), attribute.data.size());
                break;
            default:
                break;
        }
    }
    if (const auto elements = ies::elements(information_elements); elements.has_value())
        access_point.ssid = ies::ssid(*elements);
    access_point.security = security_of(information_elements, privacy);
    return access_point;
}

} // namespace

Result<std::vector<AccessPoint>> access_points(const std::string & interface_name)
{
    netlink::Netlink channel;
    auto family = nl80211::open(channel);
    SIHD_UNEXPECTED_RETURN(family);
    auto interface = nl80211::find_interface(channel, *family, interface_name);
    SIHD_UNEXPECTED_RETURN(interface);

    netlink::Request scan_dump = nl80211::request(*family, NL80211_CMD_GET_SCAN, NLM_F_REQUEST | NLM_F_DUMP);
    scan_dump.add_u32(NL80211_ATTR_IFINDEX, nl80211::ifindex(*interface));
    auto replies = channel.request(scan_dump);
    SIHD_UNEXPECTED_RETURN(replies);

    std::vector<AccessPoint> found;
    for (const netlink::Message & reply : *replies)
    {
        for (const netlink::Attribute & attribute : netlink::attributes(reply, sizeof(genlmsghdr)))
        {
            if (attribute.type == NL80211_ATTR_BSS)
                found.emplace_back(read_bss(attribute));
        }
    }
    return found;
}

} // namespace sihd::wifi
