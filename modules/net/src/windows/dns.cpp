#include <winsock2.h>

#include <sihd/net/dns.hpp>
#include <sihd/util/Error.hpp>

#include "adapters.hpp"

using namespace sihd::util;

namespace sihd::net::dns
{

Result<std::vector<DnsServer>> servers([[maybe_unused]] Duration resolver_timeout)
{
    auto adapter_list = adapters::list(GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST);
    SIHD_UNEXPECTED_RETURN(adapter_list);

    std::vector<DnsServer> servers;
    for (const IP_ADAPTER_ADDRESSES *adapter : adapters::named(*adapter_list))
    {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;
        const std::string name = adapters::friendly_name(*adapter);
        for (const IP_ADAPTER_DNS_SERVER_ADDRESS *dns = adapter->FirstDnsServerAddress; dns != nullptr; dns = dns->Next)
            servers.push_back({.addr = IpAddr(*dns->Address.lpSockaddr), .interface_name = name});
    }
    return servers;
}

} // namespace sihd::net::dns
