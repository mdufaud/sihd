#include <cstddef>
#include <cstdint>
#include <format>
#include <fuzz_helper.hpp>
#include <string>
#include <utility>
#include <vector>

#include <sihd/net/IpAddr.hpp>
#include <sihd/net/ip.hpp>

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
    const std::string dotted = std::format("{}.{}.{}.{}", octets[0], octets[1], octets[2], octets[3]);
    test::fuzz_check(ip::is_valid_ipv4(dotted), "a dotted quad is a valid ipv4");
    const IpAddr addr(dotted);
    test::fuzz_check(addr.is_ipv4(), "a dotted quad parses as ipv4");
    test::fuzz_check(addr.str() == dotted, "a dotted quad round trips through IpAddr");
    test::fuzz_check(addr.bytes() == octets, "a dotted quad keeps its octets in order");
    test::fuzz_check(IpAddr::from_bytes(AF_INET, octets) == addr, "from_bytes rebuilds the same address");
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    if (provider.ConsumeBool())
        fuzz_subnet_mask(provider);
    else
        fuzz_dotted_quad(provider);
    return 0;
}
