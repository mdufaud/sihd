#include <winsock2.h>

#include <algorithm>

#include <sihd/net/route.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Error.hpp>

#include "adapters.hpp"

using namespace sihd::util;
using enum ErrorCode;

namespace sihd::net::route
{

namespace
{

SOCKADDR_INET to_destination(const IpAddr & address)
{
    SOCKADDR_INET destination {};
    if (address.is_ipv6())
        destination.Ipv6 = address.addr6();
    else
        destination.Ipv4 = address.addr4();
    return destination;
}

// the system adds the interface metric to the route one when it picks a route
uint32_t effective_metric(const MIB_IPFORWARD_ROW2 & row)
{
    MIB_IPINTERFACE_ROW interface_row {};
    interface_row.Family = row.DestinationPrefix.Prefix.si_family;
    interface_row.InterfaceLuid = row.InterfaceLuid;
    if (GetIpInterfaceEntry(&interface_row) != NO_ERROR)
        return row.Metric;
    return row.Metric + interface_row.Metric;
}

Error route_error(unsigned long code, const std::string & context)
{
    const bool unreachable = code == ERROR_NETWORK_UNREACHABLE || code == ERROR_HOST_UNREACHABLE
                             || code == ERROR_NOT_FOUND;
    return Error(unreachable ? not_found : io_error,
                 "{}: {}",
                 context,
                 sihd::sys::os::error_str(static_cast<int>(code)));
}

Result<std::vector<Route>> default_routes(ADDRESS_FAMILY family)
{
    PMIB_IPFORWARD_TABLE2 table = nullptr;
    if (const unsigned long code = GetIpForwardTable2(family, &table); code != NO_ERROR)
        return std::unexpected(route_error(code, "could not list routes"));
    Defer free_table([table] { FreeMibTable(table); });

    std::vector<Route> routes;
    for (ULONG i = 0; i < table->NumEntries; ++i)
    {
        const MIB_IPFORWARD_ROW2 & row = table->Table[i];
        if (row.DestinationPrefix.PrefixLength != 0)
            continue;
        routes.push_back({
            .interface_name = adapters::alias_of(row.InterfaceLuid),
            .gateway = adapters::to_ip(row.NextHop),
            .source = {},
            .metric = effective_metric(row),
        });
    }
    std::ranges::stable_sort(routes, {}, &Route::metric);
    return routes;
}

} // namespace

Result<Route> find(const IpAddr & destination)
{
    if (!destination.has_ip())
        return std::unexpected(Error(invalid_argument, "route lookup needs a destination address"));

    const SOCKADDR_INET target = to_destination(destination);
    // a scoped link-local address only routes through its own link
    const NET_IFINDEX index = destination.is_ipv6() ? destination.addr6().sin6_scope_id : 0;
    MIB_IPFORWARD_ROW2 row {};
    SOCKADDR_INET source {};
    if (const unsigned long code = GetBestRoute2(nullptr, index, nullptr, &target, 0, &row, &source); code != NO_ERROR)
        return std::unexpected(route_error(code, fmt::format("route to {}", destination.str())));

    return Route {
        .interface_name = adapters::alias_of(row.InterfaceLuid),
        .gateway = adapters::to_ip(row.NextHop),
        .source = adapters::to_ip(source),
        .metric = effective_metric(row),
    };
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
