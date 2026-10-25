#ifndef __SIHD_NET_WINDOWS_ADAPTERS_HPP__
#define __SIHD_NET_WINDOWS_ADAPTERS_HPP__

// clang-format off
// netioapi.h, pulled by iphlpapi.h, declares nothing unless ws2tcpip.h came first
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
// clang-format on

#include <string>
#include <vector>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net::adapters
{

// the first entry heads the linked list GetAdaptersAddresses builds inside the buffer
using AdapterList = std::vector<IP_ADAPTER_ADDRESSES>;

sihd::util::Result<AdapterList> list(unsigned long flags);
// a nameless adapter is skipped everywhere
std::vector<const IP_ADAPTER_ADDRESSES *> named(const AdapterList & list);
std::string friendly_name(const IP_ADAPTER_ADDRESSES & adapter);
std::string alias_of(const NET_LUID & luid);
IpAddr to_ip(const SOCKADDR_INET & address);

} // namespace sihd::net::adapters

#endif
