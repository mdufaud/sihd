#ifndef __SIHD_NET_ROUTE_HPP__
#define __SIHD_NET_ROUTE_HPP__

#include <cstdint>
#include <string>
#include <vector>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net::route
{

struct Route
{
        std::string interface_name;
        IpAddr gateway;
        IpAddr source;
        // effective metric, lowest wins: 0 when unreported, as linux IPv4 lookups do
        uint32_t metric = 0;
};

// policy rules included: not_found when unreachable
sihd::util::Result<Route> find(const IpAddr & destination);
// lowest metric first
sihd::util::Result<std::vector<Route>> defaults_ipv4();
sihd::util::Result<std::vector<Route>> defaults_ipv6();

} // namespace sihd::net::route

#endif
