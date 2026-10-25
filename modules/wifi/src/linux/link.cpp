#include <sys/socket.h>

#include <optional>

#include <sihd/net/ip.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/wifi/link.hpp>

#include "nl80211.hpp"
#include <linux/genetlink.h>
#include <linux/nl80211.h>

using namespace sihd::util;
using namespace sihd::net::linux_;

namespace sihd::wifi
{

namespace
{

constexpr uint64_t bitrate_unit_bps = 100'000;

uint64_t parse_tx_bitrate_bps(const netlink::Attribute & rate_info)
{
    std::optional<uint64_t> bitrate;
    for (const netlink::Attribute & rate : netlink::nested(rate_info))
    {
        if (rate.type == NL80211_RATE_INFO_BITRATE32)
            bitrate = netlink::u32(rate) * bitrate_unit_bps;
        else if (rate.type == NL80211_RATE_INFO_BITRATE && !bitrate)
            bitrate = netlink::u16(rate) * bitrate_unit_bps;
    }
    return bitrate.value_or(0);
}

void read_station(const netlink::Message & station, Link & result)
{
    for (const netlink::Attribute & attribute : netlink::attributes(station, sizeof(genlmsghdr)))
    {
        if (attribute.type == NL80211_ATTR_MAC)
            result.bssid = net::ip::mac_str(attribute.data);
        if (attribute.type != NL80211_ATTR_STA_INFO)
            continue;
        for (const netlink::Attribute & info : netlink::nested(attribute))
        {
            if (info.type == NL80211_STA_INFO_SIGNAL)
                result.signal_dbm = static_cast<int8_t>(netlink::u8(info));
            else if (info.type == NL80211_STA_INFO_TX_BITRATE)
                result.tx_bitrate_bps = parse_tx_bitrate_bps(info);
        }
    }
}

// false unless a station associated to an access point: an access point reports its own ssid too
bool read_interface(const netlink::Message & message, Link & result)
{
    uint32_t iftype = NL80211_IFTYPE_UNSPECIFIED;
    bool has_ssid = false;
    for (const netlink::Attribute & attribute : netlink::attributes(message, sizeof(genlmsghdr)))
    {
        if (attribute.type == NL80211_ATTR_IFTYPE)
            iftype = netlink::u32(attribute);
        else if (attribute.type == NL80211_ATTR_SSID)
        {
            result.ssid.assign(attribute.data.begin(), attribute.data.end());
            has_ssid = true;
        }
        else if (attribute.type == NL80211_ATTR_WIPHY_FREQ)
            result.frequency_mhz = netlink::u32(attribute);
    }
    return has_ssid && (iftype == NL80211_IFTYPE_STATION || iftype == NL80211_IFTYPE_P2P_CLIENT);
}

} // namespace

Result<Link> link(const std::string & interface_name)
{
    netlink::Netlink channel;
    auto family = nl80211::open(channel);
    SIHD_UNEXPECTED_RETURN(family);
    auto interface = nl80211::find_interface(channel, *family, interface_name);
    SIHD_UNEXPECTED_RETURN(interface);

    Link result;
    if (!read_interface(*interface, result))
        return std::unexpected(Error(ErrorCode::not_found, "{} is not an associated wifi station", interface_name));

    netlink::Request station_request = nl80211::request(*family, NL80211_CMD_GET_STATION, NLM_F_REQUEST | NLM_F_DUMP);
    station_request.add_u32(NL80211_ATTR_IFINDEX, nl80211::ifindex(*interface));
    auto stations = channel.request(station_request);
    SIHD_UNEXPECTED_RETURN(stations);
    if (!stations->empty())
        read_station(stations->front(), result);
    return result;
}

} // namespace sihd::wifi
