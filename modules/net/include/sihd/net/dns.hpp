#ifndef __SIHD_NET_DNS_HPP__
#define __SIHD_NET_DNS_HPP__

#include <future>

#include <sihd/net/IpAddr.hpp>

namespace sihd::net::dns
{

IpAddr find(std::string_view host, bool ipv6 = false, int socktype = -1, int protocol = -1);

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

DnsLookup lookup(std::string_view host);
std::future<DnsLookup> lookup_async(std::string_view host);

} // namespace sihd::net::dns

#endif
