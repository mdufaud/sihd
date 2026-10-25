#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sys/socket.h>

# include <optional>

# include <sihd/net/ip.hpp>
# include <sihd/net/wifi.hpp>
# include <sihd/util/Error.hpp>

# include "interfaces.hpp"
# include "netlink.hpp"
# include <linux/genetlink.h>
# include <linux/nl80211.h>

using namespace sihd::util;

namespace sihd::net::wifi
{

namespace
{

const std::string nl80211_family = "nl80211";
constexpr uint64_t bitrate_unit_bps = 100'000;

Result<uint16_t> resolve_family(netlink::Netlink & channel)
{
    netlink::Request request(GENL_ID_CTRL, NLM_F_REQUEST);
    request.set_header(genlmsghdr {.cmd = CTRL_CMD_GETFAMILY, .version = 1, .reserved = 0});
    request.add_string(CTRL_ATTR_FAMILY_NAME, nl80211_family);

    auto replies = channel.request(request);
    if (!replies && replies.error().code == ErrorCode::not_found)
        return std::unexpected(Error(ErrorCode::not_supported, "nl80211 unavailable: no cfg80211 wifi stack"));
    SIHD_UNEXPECTED_RETURN(replies);

    for (const netlink::Message & message : *replies)
    {
        for (const netlink::Attribute & attribute : netlink::attributes(message, sizeof(genlmsghdr)))
        {
            if (attribute.type == CTRL_ATTR_FAMILY_ID)
                return netlink::u16(attribute);
        }
    }
    return std::unexpected(Error(ErrorCode::not_supported, "nl80211 family id missing"));
}

netlink::Request nl80211_request(uint16_t family, uint8_t command, uint16_t flags, uint32_t index)
{
    netlink::Request request(family, flags);
    request.set_header(genlmsghdr {.cmd = command, .version = 0, .reserved = 0});
    request.add_u32(NL80211_ATTR_IFINDEX, index);
    return request;
}

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
            result.bssid = ip::mac_str(attribute.data);
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
    const std::optional<uint32_t> index = interfaces::index_of(interface_name);
    if (!index)
        return std::unexpected(Error(ErrorCode::not_found, "no interface named {}", interface_name));

    netlink::Netlink channel;
    auto opened = channel.open(NETLINK_GENERIC);
    SIHD_UNEXPECTED_RETURN(opened);
    auto family = resolve_family(channel);
    SIHD_UNEXPECTED_RETURN(family);

    netlink::Request interface_request = nl80211_request(*family, NL80211_CMD_GET_INTERFACE, NLM_F_REQUEST, *index);
    auto interface_replies = channel.request(interface_request);
    if (!interface_replies && interface_replies.error().code == ErrorCode::not_found)
        return std::unexpected(Error(ErrorCode::not_found, "{} is not a wifi interface", interface_name));
    SIHD_UNEXPECTED_RETURN(interface_replies);

    Link result;
    if (interface_replies->empty() || !read_interface(interface_replies->front(), result))
        return std::unexpected(Error(ErrorCode::not_found, "{} is not an associated wifi station", interface_name));

    netlink::Request station_request = nl80211_request(*family,
                                                       NL80211_CMD_GET_STATION,
                                                       NLM_F_REQUEST | NLM_F_DUMP,
                                                       *index);
    auto stations = channel.request(station_request);
    SIHD_UNEXPECTED_RETURN(stations);
    if (!stations->empty())
        read_station(stations->front(), result);
    return result;
}

} // namespace sihd::net::wifi

#endif
