#include "adapters.hpp"

#include <array>

#include <sihd/sys/os.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;
using enum ErrorCode;

namespace sihd::net::adapters
{

Result<AdapterList> list(unsigned long flags)
{
    ULONG buf_size = 15000;
    ULONG result = ERROR_BUFFER_OVERFLOW;
    AdapterList addresses;

    while (result == ERROR_BUFFER_OVERFLOW)
    {
        addresses.resize(buf_size / sizeof(IP_ADAPTER_ADDRESSES) + 1);
        buf_size = static_cast<ULONG>(addresses.size() * sizeof(IP_ADAPTER_ADDRESSES));
        result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addresses.data(), &buf_size);
    }

    if (result != NO_ERROR)
        return std::unexpected(Error(io_error, "could not list adapters: {}", sihd::sys::os::error_str((int)result)));
    return addresses;
}

std::vector<const IP_ADAPTER_ADDRESSES *> named(const AdapterList & list)
{
    std::vector<const IP_ADAPTER_ADDRESSES *> adapters;
    for (const IP_ADAPTER_ADDRESSES *adapter = list.data(); adapter != nullptr; adapter = adapter->Next)
    {
        if (adapter->FriendlyName != nullptr)
            adapters.push_back(adapter);
    }
    return adapters;
}

std::string friendly_name(const IP_ADAPTER_ADDRESSES & adapter)
{
    return str::to_str(std::wstring_view(adapter.FriendlyName));
}

std::string alias_of(const NET_LUID & luid)
{
    std::array<wchar_t, NDIS_IF_MAX_STRING_SIZE + 1> alias {};
    if (ConvertInterfaceLuidToAlias(&luid, alias.data(), alias.size()) != NO_ERROR)
        return "";
    return str::to_str(std::wstring_view(alias.data()));
}

IpAddr to_ip(const SOCKADDR_INET & address)
{
    if (address.si_family == AF_INET && address.Ipv4.sin_addr.s_addr != 0)
        return IpAddr(address.Ipv4);
    if (address.si_family == AF_INET6 && !IN6_IS_ADDR_UNSPECIFIED(&address.Ipv6.sin6_addr))
        return IpAddr(address.Ipv6);
    return {};
}

} // namespace sihd::net::adapters
