#include "nl80211.hpp"

#include <sys/socket.h>

#include <sihd/util/Error.hpp>

#include <linux/genetlink.h>
#include <linux/nl80211.h>

using namespace sihd::util;
using namespace sihd::net::linux_;

namespace sihd::wifi::nl80211
{

namespace
{

constexpr ZStringView family_name = "nl80211";

} // namespace

Result<uint16_t> open(netlink::Netlink & channel)
{
    auto opened = channel.open(NETLINK_GENERIC);
    SIHD_UNEXPECTED_RETURN(opened);

    netlink::Request family_request(GENL_ID_CTRL, NLM_F_REQUEST);
    family_request.set_header(genlmsghdr {.cmd = CTRL_CMD_GETFAMILY, .version = 1, .reserved = 0});
    family_request.add_string(CTRL_ATTR_FAMILY_NAME, family_name);

    auto replies = channel.request(family_request);
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

netlink::Request request(uint16_t family, uint8_t command, uint16_t flags)
{
    netlink::Request built(family, flags);
    built.set_header(genlmsghdr {.cmd = command, .version = 0, .reserved = 0});
    return built;
}

Result<netlink::Message> find_interface(netlink::Netlink & channel, uint16_t family, const std::string & name)
{
    netlink::Request dump = request(family, NL80211_CMD_GET_INTERFACE, NLM_F_REQUEST | NLM_F_DUMP);
    auto interfaces = channel.request(dump);
    SIHD_UNEXPECTED_RETURN(interfaces);

    for (netlink::Message & interface : *interfaces)
    {
        for (const netlink::Attribute & attribute : netlink::attributes(interface, sizeof(genlmsghdr)))
        {
            if (attribute.type == NL80211_ATTR_IFNAME && netlink::str(attribute) == name)
                return std::move(interface);
        }
    }
    return std::unexpected(Error(ErrorCode::not_found, "no wifi interface named {}", name));
}

uint32_t ifindex(const netlink::Message & interface)
{
    for (const netlink::Attribute & attribute : netlink::attributes(interface, sizeof(genlmsghdr)))
    {
        if (attribute.type == NL80211_ATTR_IFINDEX)
            return netlink::u32(attribute);
    }
    return 0;
}

} // namespace sihd::wifi::nl80211
