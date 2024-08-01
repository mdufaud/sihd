#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/net/utils.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::net;

int main()
{
    auto opt_interfaces = NetInterface::get_all_interfaces();

    if (!opt_interfaces.has_value())
        return EXIT_FAILURE;

    for (const auto & [name, netif] : *opt_interfaces)
    {
        fmt::print("{} [{}] {}", name, netif.mac_addr(), netif.up() ? "up" : "down");
        if (netif.running())
            fmt::print(" running");
        if (netif.noarp())
            fmt::print(" noarp");
        if (netif.promisc())
            fmt::print(" promisc");
        if (netif.notrailers())
            fmt::print(" notrailers");
        if (netif.master())
            fmt::print(" master");
        if (netif.slave())
            fmt::print(" slave");
        if (netif.all_multicast())
            fmt::print(" all_multicast");
        if (netif.supports_multicast())
            fmt::print(" supports_multicast");
        if (netif.loopback())
            fmt::print(" loopback");
        fmt::printf("\n");
        if (netif.addr4().has_ip())
        {
            fmt::print("  IPV4:\n");
            fmt::print("    addr: {}\n", netif.addr4().str());
            fmt::print("    netmask: {}\n", netif.netmask4().str());
            if (netif.broadcast() || netif.point2point())
            {
                fmt::print("    {}: {}\n", netif.broadcast() ? "broadcast" : "point2point", netif.extra_addr4().str());
            }
        }
        if (netif.addr6().has_ip())
        {
            fmt::print("  IPV6:\n");
            fmt::print("    addr: {}\n", netif.addr6().str());
            fmt::print("    netmask: {}\n", netif.netmask4().str());
            if (netif.broadcast() || netif.point2point())
            {
                fmt::print("    {}: {}\n", netif.broadcast() ? "broadcast" : "point2point", netif.extra_addr6().str());
            }
        }
    }

    return EXIT_SUCCESS;
}