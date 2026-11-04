#include <sihd/util/Error.hpp>
#include <sihd/wifi/frequency.hpp>
#include <sihd/wifi/radio.hpp>

#include "nl80211.hpp"
#include <linux/genetlink.h>
#include <linux/nl80211.h>

using namespace sihd::util;
using namespace sihd::net::linux_;

namespace sihd::wifi
{

namespace
{

Radio::Mode mode_from_iftype(uint32_t iftype)
{
    switch (iftype)
    {
        case NL80211_IFTYPE_STATION:
        case NL80211_IFTYPE_P2P_CLIENT:
            return Radio::Mode::station;
        case NL80211_IFTYPE_AP:
        case NL80211_IFTYPE_AP_VLAN:
        case NL80211_IFTYPE_P2P_GO:
            return Radio::Mode::access_point;
        case NL80211_IFTYPE_ADHOC:
            return Radio::Mode::ad_hoc;
        case NL80211_IFTYPE_MESH_POINT:
            return Radio::Mode::mesh;
        case NL80211_IFTYPE_MONITOR:
            return Radio::Mode::monitor;
        default:
            return Radio::Mode::unknown;
    }
}

} // namespace

Result<Radio> radio(const std::string & interface_name)
{
    netlink::Netlink channel;
    auto family = nl80211::open(channel);
    SIHD_UNEXPECTED_RETURN(family);
    auto interface = nl80211::find_interface(channel, *family, interface_name);
    SIHD_UNEXPECTED_RETURN(interface);

    Radio result;
    for (const netlink::Attribute & attribute : netlink::attributes(*interface, sizeof(genlmsghdr)))
    {
        if (attribute.type == NL80211_ATTR_IFTYPE)
            result.mode = mode_from_iftype(netlink::u32(attribute));
        else if (attribute.type == NL80211_ATTR_WIPHY_FREQ)
            result.frequency_mhz = netlink::u32(attribute);
    }
    result.channel = channel_from_frequency(result.frequency_mhz).value_or(0);
    return result;
}

} // namespace sihd::wifi
