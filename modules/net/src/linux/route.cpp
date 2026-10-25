#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sys/socket.h>

# include <algorithm>

# include <sihd/net/linux/netlink.hpp>
# include <sihd/net/route.hpp>
# include <sihd/util/Error.hpp>

# include "interfaces.hpp"
# include <linux/rtnetlink.h>

using namespace sihd::util;
using namespace sihd::net::linux_;

namespace sihd::net::route
{

namespace
{

Route parse_route(const netlink::Message & message)
{
    const rtmsg header = netlink::header<rtmsg>(message);
    Route route;
    for (const netlink::Attribute & attribute : netlink::attributes(message, sizeof(rtmsg)))
    {
        switch (attribute.type)
        {
            case RTA_OIF:
                route.interface_name = interfaces::name_of(netlink::u32(attribute)).value_or("");
                break;
            case RTA_GATEWAY:
                route.gateway = netlink::ip(attribute, header.rtm_family);
                break;
            case RTA_PREFSRC:
                route.source = netlink::ip(attribute, header.rtm_family);
                break;
            case RTA_PRIORITY:
                route.metric = netlink::u32(attribute);
                break;
            default:
                break;
        }
    }
    return route;
}

Result<std::vector<Route>> default_routes(uint8_t family)
{
    netlink::Netlink channel;
    auto opened = channel.open(NETLINK_ROUTE);
    SIHD_UNEXPECTED_RETURN(opened);

    rtmsg header {};
    header.rtm_family = family;
    netlink::Request request(RTM_GETROUTE, NLM_F_REQUEST | NLM_F_DUMP);
    request.set_header(header);

    auto replies = channel.request(request);
    SIHD_UNEXPECTED_RETURN(replies);

    std::vector<Route> routes;
    for (const netlink::Message & message : *replies)
    {
        const rtmsg route_header = netlink::header<rtmsg>(message);
        if (message.type == RTM_NEWROUTE && route_header.rtm_dst_len == 0 && route_header.rtm_type == RTN_UNICAST)
            routes.push_back(parse_route(message));
    }
    std::ranges::stable_sort(routes, {}, &Route::metric);
    return routes;
}

} // namespace

Result<Route> find(const IpAddr & destination)
{
    if (!destination.has_ip())
        return std::unexpected(Error(ErrorCode::invalid_argument, "route lookup needs a destination address"));

    netlink::Netlink channel;
    auto opened = channel.open(NETLINK_ROUTE);
    SIHD_UNEXPECTED_RETURN(opened);

    const bool ipv6 = destination.is_ipv6();
    rtmsg header {};
    header.rtm_family = ipv6 ? AF_INET6 : AF_INET;
    header.rtm_dst_len = ipv6 ? 128 : 32;
    netlink::Request request(RTM_GETROUTE, NLM_F_REQUEST);
    request.set_header(header);
    const std::vector<uint8_t> destination_bytes = destination.bytes();
    request.add_attribute(RTA_DST, destination_bytes.data(), destination_bytes.size());
    // a scoped link-local address only routes through its own link
    if (ipv6 && destination.addr6().sin6_scope_id != 0)
        request.add_u32(RTA_OIF, destination.addr6().sin6_scope_id);

    auto replies = channel.request(request);
    SIHD_UNEXPECTED_RETURN_CTX(replies, "route to {}", destination.str());

    for (const netlink::Message & message : *replies)
    {
        if (message.type == RTM_NEWROUTE)
            return parse_route(message);
    }
    return std::unexpected(Error(ErrorCode::not_found, "no route to {}", destination.str()));
}

Result<std::vector<Route>> defaults_ipv4()
{
    return default_routes(AF_INET);
}

Result<std::vector<Route>> defaults_ipv6()
{
    return default_routes(AF_INET6);
}

} // namespace sihd::net::route

#endif
