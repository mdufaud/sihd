#ifndef __SIHD_NET_DNS_HPP__
#define __SIHD_NET_DNS_HPP__

#include <future>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net::dns
{

sihd::util::Result<IpAddr> find(const std::string & host, bool ipv6 = false, int socktype = -1, int protocol = -1);

struct DnsEntry
{
        IpAddr addr;
        int socktype;
        int protocol;
};

struct DnsLookup
{
        bool success = false;
        std::string host;
        std::string hostname;
        std::vector<DnsEntry> results;

        std::string dump() const;
};

DnsLookup lookup(const std::string & host);
std::future<DnsLookup> lookup_async(const std::string & host);

} // namespace sihd::net::dns

#endif
