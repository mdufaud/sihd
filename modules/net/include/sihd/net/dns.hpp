#ifndef __SIHD_NET_DNS_HPP__
#define __SIHD_NET_DNS_HPP__

#include <future>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>
#include <sihd/util/time.hpp>

namespace sihd::net::dns
{

sihd::util::Result<IpAddr> find(sihd::util::ZStringView host, bool ipv6 = false, int socktype = -1, int protocol = -1);

struct DnsEntry
{
        IpAddr addr;
        int socktype;
        int protocol;
};

struct DnsLookup
{
        std::string host;
        std::string hostname;
        std::vector<DnsEntry> results;

        std::string dump() const;
};

sihd::util::Result<DnsLookup> lookup(const std::string & host);
std::future<sihd::util::Result<DnsLookup>> lookup_async(const std::string & host);

struct DnsServer
{
        IpAddr addr;
        // empty when the server applies to every interface
        std::string interface_name;
};

inline constexpr sihd::util::Duration default_resolver_timeout {sihd::util::time::sec(2)};

// resolver_timeout bounds the systemd-resolved query, when resolv.conf only lists its stub
sihd::util::Result<std::vector<DnsServer>> servers(sihd::util::Duration resolver_timeout = default_resolver_timeout);

} // namespace sihd::net::dns

#endif
