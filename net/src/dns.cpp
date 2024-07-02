#include <strings.h>

#include <functional>

#include <sihd/net/dns.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netdb.h>      // getnameinfo
# include <netinet/in.h> // sockaddr
# include <sys/socket.h> // getnameinfo
#else
# include <winsock2.h>
# include <ws2tcpip.h> // addrinfo
#endif

namespace sihd::net::dns
{

SIHD_NEW_LOGGER("sihd::net::dns");

using namespace sihd::util;

namespace
{

bool ask_dns(std::string_view host, std::function<bool(const IpAddr &, int, int)> && callback)
{
    int ret;
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *first_result;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_CANONNAME;

    if ((ret = getaddrinfo(host.data(), NULL, &hints, &results)) != 0)
    {
        SIHD_LOG(error, "getaddrinfo error '{}': {}", host, gai_strerror(ret));
        return false;
    }

    first_result = results;
    Defer d([&first_result] { freeaddrinfo(first_result); });

    std::string hostname;
    while (results)
    {
        // cannoname is only in first result
        if (results == first_result && results->ai_canonname != nullptr)
            hostname = results->ai_canonname;

        const int protocol = results->ai_protocol;
        const int socktype = results->ai_socktype;

        const sockaddr *addr = results->ai_addr;
        const size_t addr_len = results->ai_addrlen;

        IpAddr ipaddr(*addr, addr_len);
        ipaddr.set_hostname(hostname);

        if (ipaddr.has_ip() && callback(ipaddr, socktype, protocol))
        {
            break;
        }

        results = results->ai_next;
    }
    return true;
}

} // namespace

IpAddr find(std::string_view host, bool ipv6, int socktype, int protocol)
{
    IpAddr ret;

    ask_dns(host, [&ret, &ipv6, &socktype, &protocol](const IpAddr & addr, int addr_socktype, int addr_protocol) {
        const bool good_socktype = socktype < 0 || socktype == addr_socktype;
        const bool good_protocol = protocol < 0 || protocol == addr_protocol;
        const bool good_iptype = ipv6 == addr.is_ipv6();

        const bool found = good_iptype && good_protocol && good_socktype;

        if (found)
            ret = addr;

        return found;
    });

    return ret;
}

DnsLookup lookup(std::string_view host)
{
    DnsLookup ret;

    ret.host = host;
    ret.success = ask_dns(host, [&ret](const IpAddr & addr, int addr_socktype, int addr_protocol) {
        if (ret.hostname.empty())
            ret.hostname = addr.hostname();

        DnsEntry entry;
        entry.addr = addr;
        entry.socktype = addr_socktype;
        entry.protocol = addr_protocol;
        ret.results.emplace_back(std::move(entry));
        // get all results by sending false
        return false;
    });

    return ret;
}

std::string DnsLookup::dump() const
{
    std::string dump = fmt::format("host: {}\n", host);

    for (const DnsEntry & entry : results)
    {
        dump += fmt::format("ipv{} {} {}: {} ({})\n",
                            (entry.addr.is_ipv6() ? '6' : '4'),
                            ip::protocol_str(entry.protocol),
                            ip::socktype_str(entry.socktype),
                            entry.addr.hostname(),
                            entry.addr.str());
    }
    return dump;
}

} // namespace sihd::net::dns
