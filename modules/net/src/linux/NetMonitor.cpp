#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sys/socket.h>

# include <optional>
# include <stdexcept>
# include <string>
# include <unordered_map>

# include <sihd/net/NetMonitor.hpp>
# include <sihd/net/linux/netlink.hpp>
# include <sihd/sys/Poll.hpp>
# include <sihd/util/Error.hpp>

# include "interfaces.hpp"
# include <linux/rtnetlink.h>

using namespace sihd::util;
using namespace sihd::net::linux_;

namespace sihd::net
{

namespace
{

constexpr uint32_t monitored_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_IPV4_ROUTE
                                      | RTMGRP_IPV6_ROUTE;

// a removed interface no longer resolves: the names of the links seen so far still name its last events
using InterfaceNames = std::unordered_map<uint32_t, std::string>;

uint32_t link_index(const netlink::Message & message)
{
    return static_cast<uint32_t>(netlink::header<ifinfomsg>(message).ifi_index);
}

std::string link_name(const netlink::Message & message)
{
    for (const netlink::Attribute & attribute : netlink::attributes(message, sizeof(ifinfomsg)))
    {
        if (attribute.type == IFLA_IFNAME)
            return netlink::str(attribute);
    }
    return "";
}

std::string name_of(const InterfaceNames & names, uint32_t index)
{
    const auto it = names.find(index);
    if (it != names.end())
        return it->second;
    return interfaces::name_of(index).value_or("");
}

// a refused dump (android apps) leaves the names to the link events
InterfaceNames dump_link_names()
{
    InterfaceNames names;
    netlink::Netlink channel;
    if (!channel.open(NETLINK_ROUTE))
        return names;
    netlink::Request request(RTM_GETLINK, NLM_F_REQUEST | NLM_F_DUMP);
    request.set_header(ifinfomsg {});
    auto replies = channel.request(request);
    if (!replies)
        return names;
    for (const netlink::Message & message : *replies)
    {
        std::string name = link_name(message);
        if (message.type == RTM_NEWLINK && !name.empty())
            names[link_index(message)] = std::move(name);
    }
    return names;
}

NetMonitorEvent link_event(const netlink::Message & message, InterfaceNames & names)
{
    NetMonitorEvent event;
    event.type = NetMonitorEventType::link;
    event.removed = message.type == RTM_DELLINK;
    const uint32_t index = link_index(message);
    event.interface_name = link_name(message);
    if (event.interface_name.empty())
        event.interface_name = name_of(names, index);
    // the kernel sends the addresses and routes of a removed link before its removal
    if (event.removed)
        names.erase(index);
    else if (!event.interface_name.empty())
        names[index] = event.interface_name;
    return event;
}

NetMonitorEvent address_event(const netlink::Message & message, const InterfaceNames & names)
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
        event.interface_name = name_of(names, header.ifa_index);
    return event;
}

NetMonitorEvent route_event(const netlink::Message & message, const InterfaceNames & names)
{
    NetMonitorEvent event;
    event.type = NetMonitorEventType::route;
    event.removed = message.type == RTM_DELROUTE;
    for (const netlink::Attribute & attribute : netlink::attributes(message, sizeof(rtmsg)))
    {
        if (attribute.type == RTA_OIF)
            event.interface_name = name_of(names, netlink::u32(attribute));
    }
    return event;
}

std::optional<NetMonitorEvent> to_event(const netlink::Message & message, InterfaceNames & names)
{
    switch (message.type)
    {
        case RTM_NEWLINK:
        case RTM_DELLINK:
            return link_event(message, names);
        case RTM_NEWADDR:
        case RTM_DELADDR:
            return address_event(message, names);
        case RTM_NEWROUTE:
        case RTM_DELROUTE:
            return route_event(message, names);
        default:
            return std::nullopt;
    }
}

bool drain_events(netlink::Netlink & channel, InterfaceNames & names, std::vector<NetMonitorEvent> & events)
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
            if (auto event = to_event(message, names))
                events.push_back(std::move(*event));
        }
    }
}

} // namespace

struct NetMonitor::Impl
{
        netlink::Netlink channel;
        sihd::sys::Poll poll {1};
        InterfaceNames names;

        static Result<std::unique_ptr<Impl>> open()
        {
            auto impl = std::make_unique<Impl>();
            auto opened = impl->channel.open(NETLINK_ROUTE);
            SIHD_UNEXPECTED_RETURN(opened);
            auto subscribed = impl->channel.subscribe(monitored_groups);
            SIHD_UNEXPECTED_RETURN(subscribed);
            auto nonblocking = impl->channel.set_nonblocking();
            SIHD_UNEXPECTED_RETURN(nonblocking);
            impl->poll.set_read_fd(impl->channel.fd());
            // dumped once subscribed: a link created in between is in the dump or in the events
            impl->names = dump_link_names();
            return impl;
        }
};

NetMonitor::NetMonitor()
{
    auto impl = Impl::open();
    if (!impl)
        throw std::runtime_error(impl.error().message);
    _impl = std::move(*impl);
}

NetMonitor::~NetMonitor() = default;

bool NetMonitor::run()
{
    _events.clear();
    const int ready = _impl->poll.poll(_run_timeout.milliseconds_int());
    if (ready < 0)
        return false;
    if (ready > 0 && !drain_events(_impl->channel, _impl->names, _events))
        return false;

    if (!_events.empty())
        this->notify_observers(this);
    return true;
}

} // namespace sihd::net

#endif
