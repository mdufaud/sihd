#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fuzz_helper.hpp>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/platform.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::net;

void fuzz_subnet_mask(FuzzedDataProvider & provider)
{
    const uint32_t first = provider.ConsumeIntegral<uint32_t>();
    const uint32_t second = provider.ConsumeIntegral<uint32_t>();
    for (const auto & [host, max_mask] :
         {std::pair<const char *, uint32_t> {"10.0.0.1", 32}, std::pair<const char *, uint32_t> {"2001:db8::1", 128}})
    {
        IpAddr addr(host);
        const bool first_ok = addr.set_subnet_mask(first);
        test::fuzz_check(first_ok == (first <= max_mask), "a mask is accepted up to the address width");
        if (first_ok)
            test::fuzz_check(addr.subnet_value() == first, "an accepted mask is stored");

        test::fuzz_check(addr.has_subnet() == (addr.subnet_value() > 0), "a subnet is a non zero mask");

        const bool had_subnet = addr.has_subnet();
        const uint32_t previous = addr.subnet_value();
        if (!addr.set_subnet_mask(second))
        {
            test::fuzz_check(addr.has_subnet() == had_subnet, "a rejected mask keeps the subnet state");
            test::fuzz_check(addr.subnet_value() == previous, "a rejected mask keeps the previous value");
        }
    }
}

void fuzz_dotted_quad(FuzzedDataProvider & provider)
{
    const std::vector<uint8_t> octets = provider.ConsumeBytes<uint8_t>(4);
    if (octets.size() != 4)
        return;
    const std::string dotted = fmt::format("{}.{}.{}.{}", octets[0], octets[1], octets[2], octets[3]);
    test::fuzz_check(ip::is_valid_ipv4(dotted), "a dotted quad is a valid ipv4");
    const IpAddr addr(dotted);
    test::fuzz_check(addr.is_ipv4(), "a dotted quad parses as ipv4");
    test::fuzz_check(addr.str() == dotted, "a dotted quad round trips through IpAddr");
    test::fuzz_check(addr.bytes() == octets, "a dotted quad keeps its octets in order");
    test::fuzz_check(IpAddr::from_bytes(AF_INET, octets) == addr, "from_bytes rebuilds the same address");
}

std::string ipv6_text(const std::vector<uint8_t> & octets)
{
    std::string text;
    for (size_t i = 0; i < octets.size(); i += 2)
        text += fmt::format("{}{:x}", i == 0 ? "" : ":", octets[i] << 8 | octets[i + 1]);
    return text;
}

void check_masked_text(const std::vector<uint8_t> & octets, int family, uint32_t mask)
{
    const std::string host = family == AF_INET ? fmt::format("{}.{}.{}.{}", octets[0], octets[1], octets[2], octets[3])
                                               : ipv6_text(octets);
    const uint32_t max_mask = family == AF_INET ? 32 : 128;

    const IpAddr masked(fmt::format("{}/{}", host, mask));
    test::fuzz_check(masked.has_ip() && masked.bytes() == octets, "an address followed by a mask keeps its bytes");
    test::fuzz_check(masked.subnet_value() == (mask <= max_mask ? mask : 0),
                     "a mask over the address width is dropped, else kept");
    test::fuzz_check(masked.has_subnet() == (mask > 0 && mask <= max_mask), "a masked address has a subnet");

    const IpAddr garbage(fmt::format("{}/x{}", host, mask));
    test::fuzz_check(garbage.bytes() == octets && !garbage.has_subnet(), "a text mask that is not a number is dropped");

    const IpAddr plain(host);
    test::fuzz_check(plain.bytes() == octets && !plain.has_subnet(), "an address without mask has no subnet");
    test::fuzz_check(!IpAddr(host + "/").has_subnet(), "an empty mask is dropped");
}

void fuzz_masked_text(FuzzedDataProvider & provider)
{
    const bool ipv6 = provider.ConsumeBool();
    const uint32_t mask = provider.ConsumeIntegralInRange<uint32_t>(0, 140);
    const std::vector<uint8_t> octets = test::consume_exact_bytes(provider, ipv6 ? 16 : 4);
    check_masked_text(octets, ipv6 ? AF_INET6 : AF_INET, mask);
}

size_t family_size(int family)
{
    if (family == AF_INET)
        return sizeof(sockaddr_in);
    if (family == AF_INET6)
        return sizeof(sockaddr_in6);
    return sizeof(sockaddr);
}

bool sockaddr_accepted(int family, size_t length)
{
    if (family == AF_INET)
        return length == 0 || length == sizeof(sockaddr_in);
    if (family == AF_INET6)
        return length == 0 || length == sizeof(sockaddr_in6);
    return false;
}

void fuzz_sockaddr(FuzzedDataProvider & provider)
{
    const std::array<int, 4> families = {AF_INET, AF_INET6, AF_UNSPEC, AF_UNIX};
    const std::array<size_t, 5> lengths = {0, sizeof(sockaddr_in), sizeof(sockaddr_in6), 1, sizeof(sockaddr)};
    const int family = provider.ConsumeBool() ? provider.PickValueInArray(families)
                                              : static_cast<int>(provider.ConsumeIntegral<uint16_t>());
    const size_t length = provider.ConsumeBool() ? provider.PickValueInArray(lengths)
                                                 : provider.ConsumeIntegralInRange<size_t>(0, 40);

    // exact size heap buffer: a read of the wrong sockaddr size is seen by ASan
    std::vector<uint8_t> raw = test::consume_exact_bytes(provider, family_size(family));
    const sa_family_t raw_family = static_cast<sa_family_t>(family);
    std::memcpy(raw.data(), &raw_family, sizeof(raw_family));

    const IpAddr addr(*reinterpret_cast<const sockaddr *>(raw.data()), length);
    if (!sockaddr_accepted(family, length))
    {
        test::fuzz_check(addr.empty(), "a sockaddr of another family or length is refused");
        return;
    }

    const size_t address_offset = family == AF_INET ? offsetof(sockaddr_in, sin_addr)
                                                    : offsetof(sockaddr_in6, sin6_addr);
    const size_t address_size = family == AF_INET ? sizeof(in_addr) : sizeof(in6_addr);
    const size_t port_offset = family == AF_INET ? offsetof(sockaddr_in, sin_port) : offsetof(sockaddr_in6, sin6_port);
    uint16_t network_port = 0;
    std::memcpy(&network_port, raw.data() + port_offset, sizeof(network_port));

    test::fuzz_check(addr.is_ipv4() == (family == AF_INET) && addr.is_ipv6() == (family == AF_INET6),
                     "an accepted sockaddr keeps its family");
    test::fuzz_check(addr.port() == ntohs(network_port), "an accepted sockaddr keeps its port");
    test::fuzz_check(addr.addr_len() == family_size(family), "an accepted sockaddr has the size of its family");
    test::fuzz_check(
        addr.bytes() == std::vector<uint8_t>(raw.begin() + address_offset, raw.begin() + address_offset + address_size),
        "an accepted sockaddr keeps its address");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    switch (provider.ConsumeIntegralInRange<int>(0, 3))
    {
        case 0:
            fuzz_subnet_mask(provider);
            break;
        case 1:
            fuzz_dotted_quad(provider);
            break;
        case 2:
            fuzz_masked_text(provider);
            break;
        default:
            fuzz_sockaddr(provider);
            break;
    }
    return 0;
}
