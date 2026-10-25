#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/ranges.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/net/NetMonitor.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/net/route.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::net;
using namespace sihd::util;

namespace
{

using Interfaces = std::map<std::string, NetInterface>;

constexpr std::string_view ipv4_probe = "1.1.1.1";
constexpr std::string_view ipv6_probe = "2606:4700:4700::1111";

void print_flags(const NetInterface & netif)
{
    const std::vector<std::pair<bool, std::string_view>> flags = {
        {netif.running(), "running"},
        {netif.loopback(), "loopback"},
        {netif.point2point(), "point2point"},
        {netif.noarp(), "noarp"},
        {netif.promisc(), "promisc"},
        {netif.notrailers(), "notrailers"},
        {netif.master(), "master"},
        {netif.slave(), "slave"},
        {netif.all_multicast(), "all_multicast"},
        {netif.supports_multicast(), "supports_multicast"},
    };
    for (const auto & [set, name] : flags)
    {
        if (set)
            fmt::print(" {}", name);
    }
}

void print_interface(const NetInterface & netif)
{
    fmt::print("{} [{}] {} {} index={} mtu={}",
               netif.name(),
               netif.mac_addr(),
               NetInterface::kind_str(netif.kind()),
               netif.hardware() ? "hardware" : "virtual",
               netif.index(),
               netif.mtu());
    if (netif.link_speed_bps() > 0)
        fmt::print(" speed={}Mb/s", netif.link_speed_bps() / 1'000'000);
    fmt::print(" {}", netif.up() ? "up" : "down");
    print_flags(netif);
    fmt::print("\n");

    for (const NetInterface::Address & address : netif.addresses())
    {
        fmt::print("  {} netmask {}", address.addr.str(), address.netmask.str());
        if (address.extra_addr.has_ip())
            fmt::print(" {} {}", netif.point2point() ? "peer" : "broadcast", address.extra_addr.str());
        fmt::print("\n");
    }
}

void print_route(std::string_view label, const route::Route & found)
{
    fmt::print("{}: {}", label, found.interface_name);
    if (found.gateway.has_ip())
        fmt::print(" via {}", found.gateway.str());
    if (found.source.has_ip())
        fmt::print(" src {}", found.source.str());
    if (found.metric > 0)
        fmt::print(" metric {}", found.metric);
    fmt::print("\n");
}

void print_routes(std::string_view label, const Result<std::vector<route::Route>> & routes)
{
    if (!routes)
    {
        fmt::print("{}: {}\n", label, routes.error().message);
        return;
    }
    for (const route::Route & found : *routes)
        print_route(label, found);
}

void print_route_to(std::string_view destination, const Result<route::Route> & found)
{
    if (found)
        print_route(fmt::format("route to {}", destination), *found);
    else
        fmt::print("route to {}: {}\n", destination, found.error().message);
}

bool is_vpn_link(const Interfaces & interfaces, const std::string & name)
{
    const auto it = interfaces.find(name);
    if (it == interfaces.end())
        return false;
    return it->second.kind() == NetInterface::Kind::tunnel || it->second.kind() == NetInterface::Kind::ppp;
}

std::string link_label(const Interfaces & interfaces, const std::string & name)
{
    const auto it = interfaces.find(name);
    if (it == interfaces.end())
        return name;
    return fmt::format("{} ({})", name, NetInterface::kind_str(it->second.kind()));
}

void print_vpn_verdict(const Interfaces & interfaces,
                       const route::Route & primary,
                       const std::vector<route::Route> & defaults)
{
    if (is_vpn_link(interfaces, primary.interface_name))
    {
        std::string uplink = "unknown";
        for (const route::Route & default_route : defaults)
        {
            if (!is_vpn_link(interfaces, default_route.interface_name))
            {
                uplink = link_label(interfaces, default_route.interface_name);
                break;
            }
        }
        fmt::print("connection: full vpn tunnel through {}, uplink {}\n",
                   link_label(interfaces, primary.interface_name),
                   uplink);
        return;
    }

    std::vector<std::string> tunnels;
    for (const auto & [name, netif] : interfaces)
    {
        if (netif.running() && is_vpn_link(interfaces, name))
            tunnels.push_back(name);
    }
    fmt::print("connection: {}{}\n",
               link_label(interfaces, primary.interface_name),
               tunnels.empty() ? "" : fmt::format(", split vpn tunnel on {}", fmt::join(tunnels, ", ")));
}

void print_dns_servers()
{
    auto servers = dns::servers();
    if (!servers)
    {
        fmt::print("dns: {}\n", servers.error().message);
        return;
    }
    for (const dns::DnsServer & server : *servers)
    {
        auto route_to_server = route::find(server.addr);
        fmt::print("dns: {} ({}) reached through {}\n",
                   server.addr.str(),
                   server.interface_name.empty() ? "global" : server.interface_name,
                   route_to_server ? route_to_server->interface_name : route_to_server.error().message);
    }
}

void print_connection(const Interfaces & interfaces)
{
    const Result<route::Route> ipv4_primary = route::find(IpAddr(ipv4_probe));
    const Result<route::Route> ipv6_primary = route::find(IpAddr(ipv6_probe));
    const Result<std::vector<route::Route>> ipv4_defaults = route::defaults_ipv4();
    const Result<std::vector<route::Route>> ipv6_defaults = route::defaults_ipv6();

    print_route_to(ipv4_probe, ipv4_primary);
    print_route_to(ipv6_probe, ipv6_primary);
    print_routes("default ipv4", ipv4_defaults);
    print_routes("default ipv6", ipv6_defaults);
    if (ipv4_primary)
        print_vpn_verdict(interfaces, *ipv4_primary, ipv4_defaults.value_or(std::vector<route::Route> {}));
    else if (ipv6_primary)
        print_vpn_verdict(interfaces, *ipv6_primary, ipv6_defaults.value_or(std::vector<route::Route> {}));
    else
        fmt::print("connection: offline\n");
    print_dns_servers();
}

void watch_changes(sihd::sys::App & app)
{
    NetMonitor monitor(std::chrono::milliseconds(500));
    Handler<NetMonitor *> printer([](NetMonitor *changed) {
        for (const NetMonitorEvent & event : changed->events())
        {
            fmt::print("{} {}{} {}\n",
                       event.type_str(),
                       event.removed ? "removed " : "",
                       event.interface_name,
                       event.address.str());
        }
    });
    monitor.add_observer(&printer);
    bool failed = false;
    app.loop(std::nullopt, [&monitor, &failed] {
        failed = !monitor.run();
        return !failed;
    });
    if (failed)
        app.exit(EXIT_FAILURE);
}

} // namespace

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "net_interfaces_demo",
        .description = "Dump network interfaces, routes, vpn state and dns servers",
    });

    app.root().on_run([&app] {
        auto interfaces = NetInterface::get_all_interfaces();
        if (!interfaces)
        {
            fmt::print(stderr, "{}\n", interfaces.error().message);
            app.exit(EXIT_FAILURE);
            return;
        }

        for (const auto & [name, netif] : *interfaces)
            print_interface(netif);
        print_connection(*interfaces);
    });
    app.root().add_command("watch", "print network changes until interrupted").on_run([&app] { watch_changes(app); });

    return app.run(argc, argv);
}
