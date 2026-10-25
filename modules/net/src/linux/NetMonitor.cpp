#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sys/socket.h>

# include <optional>
# include <stdexcept>
# include <string>

# include <sihd/net/NetMonitor.hpp>
# include <sihd/sys/Poll.hpp>
# include <sihd/util/Error.hpp>

# include "interfaces.hpp"
# include "netlink.hpp"
# include <linux/rtnetlink.h>

using namespace sihd::util;

namespace sihd::net
{

namespace
{

constexpr uint32_t monitored_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_IPV4_ROUTE
                                      | RTMGRP_IPV6_ROUTE;

NetMonitorEvent link_event(const netlink::Message & message)
{
    const ifinfomsg header = netlink::header<ifinfomsg>(message);
    NetMonitorEvent event;
    event.type = NetMonitorEventType::link;
    event.removed = message.type == RTM_DELLINK;
    for (const netlink::Attribute & attribute : netlink::attributes(message, sizeof(ifinfomsg)))
    {
        if (attribute.type == IFLA_IFNAME)
            event.interface_name = netlink::str(attribute);
    }
    if (event.interface_name.empty())
        event.interface_name = interfaces::name_of(static_cast<uint32_t>(header.ifi_index)).value_or("");
    return event;
}

NetMonitorEvent address_event(const netlink::Message & message)
{
    const ifaddrmsg header = netlink::header<ifaddrmsg>(message);
    NetMonitorEvent event;
    event.type = NetMonitorEventType::address;
    event.removed = message.type == RTM_DELADDR;
    IpAddr peer;

    for (const netlink::Attribute & attribute : netlink::attributes(message, sizeof(ifaddrmsg)))
    {
        // IFA_LOCAL is the own address of a point-to-point link, whose IFA_ADDRESS is the peer
        if (attribute.type == IFA_LOCAL)
            event.address = netlink::ip(attribute, header.ifa_family);
        else if (attribute.type == IFA_ADDRESS)
            peer = netlink::ip(attribute, header.ifa_family);
        else if (attribute.type == IFA_LABEL)
        {
            const std::string label = netlink::str(attribute);
            event.interface_name = label.substr(0, label.find(':'));
        }
    }

    if (!event.address.has_ip())
        event.address = std::move(peer);
    if (event.interface_name.empty())
        event.interface_name = interfaces::name_of(header.ifa_index).value_or("");
    return event;
}

NetMonitorEvent route_event(const netlink::Message & message)
{
    NetMonitorEvent event;
    event.type = NetMonitorEventType::route;
    event.removed = message.type == RTM_DELROUTE;
    for (const netlink::Attribute & attribute : netlink::attributes(message, sizeof(rtmsg)))
    {
        if (attribute.type == RTA_OIF)
            event.interface_name = interfaces::name_of(netlink::u32(attribute)).value_or("");
    }
    return event;
}

std::optional<NetMonitorEvent> to_event(const netlink::Message & message)
{
    switch (message.type)
    {
        case RTM_NEWLINK:
        case RTM_DELLINK:
            return link_event(message);
        case RTM_NEWADDR:
        case RTM_DELADDR:
            return address_event(message);
        case RTM_NEWROUTE:
        case RTM_DELROUTE:
            return route_event(message);
        default:
            return std::nullopt;
    }
}

bool drain_events(netlink::Netlink & channel, std::vector<NetMonitorEvent> & events)
{
    while (true)
    {
        auto messages = channel.receive();
        if (!messages && messages.error().code == ErrorCode::would_block)
            return true;
        // ENOBUFS: the kernel dropped notifications the socket had no room for
        if (!messages && messages.error().code == ErrorCode::out_of_memory)
        {
            NetMonitorEvent overflow;
            overflow.type = NetMonitorEventType::overflow;
            events.push_back(std::move(overflow));
            return true;
        }
        if (!messages)
            return false;
        for (const netlink::Message & message : *messages)
        {
            if (auto event = to_event(message))
                events.push_back(std::move(*event));
        }
    }
}

} // namespace

struct NetMonitor::Impl
{
        netlink::Netlink channel;
        sihd::sys::Poll poll {1};
};

NetMonitor::NetMonitor(): _impl(std::make_unique<Impl>())
{
    auto opened = _impl->channel.open(NETLINK_ROUTE);
    if (!opened)
        throw std::runtime_error(opened.error().message);
    auto subscribed = _impl->channel.subscribe(monitored_groups);
    if (!subscribed)
        throw std::runtime_error(subscribed.error().message);
    auto nonblocking = _impl->channel.set_nonblocking();
    if (!nonblocking)
        throw std::runtime_error(nonblocking.error().message);
    _impl->poll.set_read_fd(_impl->channel.fd());
}

NetMonitor::~NetMonitor() = default;

bool NetMonitor::run()
{
    _events.clear();
    const int ready = _impl->poll.poll(_run_timeout.milliseconds_int());
    if (ready < 0)
        return false;
    if (ready > 0 && !drain_events(_impl->channel, _events))
        return false;

    if (!_events.empty())
        this->notify_observers(this);
    return true;
}

} // namespace sihd::net

#endif
