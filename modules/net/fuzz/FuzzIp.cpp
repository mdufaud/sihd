#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <string>

#include <sihd/net/ip.hpp>

#include <fuzzer/FuzzedDataProvider.h>

namespace
{
using namespace sihd::net;

struct Cidr
{
        uint32_t base;
        unsigned int prefix;
};

constexpr Cidr private_ranges[] = {
    {0x00000000, 8},  // 0.0.0.0
    {0x7F000000, 8},  // 127.0.0.0
    {0x0A000000, 8},  // 10.0.0.0
    {0x64400000, 10}, // 100.64.0.0
    {0xA9FE0000, 16}, // 169.254.0.0
    {0xAC100000, 12}, // 172.16.0.0
    {0xC0000000, 24}, // 192.0.0.0
    {0xC0A80000, 16}, // 192.168.0.0
    {0xC6120000, 15}, // 198.18.0.0
    {0xE0000000, 4},  // 224.0.0.0
    {0xF0000000, 4},  // 240.0.0.0
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);

    const uint32_t addr = provider.ConsumeIntegral<uint32_t>();
    bool expected = false;
    for (const Cidr & range : private_ranges)
        expected = expected || (addr & (~uint32_t {0} << (32 - range.prefix))) == range.base;
    test::fuzz_check(ip::is_private_ipv4(addr) == expected, "is_private_ipv4 matches the reserved ranges");

    const std::string text = provider.ConsumeRandomLengthString(64);
    test::fuzz_check(ip::is_valid_ip(text) == (ip::is_valid_ipv4(text) || ip::is_valid_ipv6(text)),
                     "is_valid_ip is ipv4 or ipv6");
    test::fuzz_check(!(ip::is_valid_ipv4(text) && ip::is_valid_ipv6(text)), "a string is not both ipv4 and ipv6");

    const uint32_t bits = provider.ConsumeIntegral<uint32_t>();
    test::fuzz_check(std::popcount(ip::to_netmask(bits)) == static_cast<int>(std::min<uint32_t>(bits, 32)),
                     "to_netmask sets as many bits as asked, up to 32");

    const uint32_t mask = provider.ConsumeBool()
                              ? provider.ConsumeIntegral<uint32_t>()
                              : ~uint32_t {0} << provider.ConsumeIntegralInRange<unsigned int>(0, 31);
    bool contiguous = false;
    for (unsigned int ones = 1; ones <= 32; ++ones)
        contiguous = contiguous || mask == ~uint32_t {0} << (32 - ones);
    test::fuzz_check(ip::is_valid_netmask(mask) == contiguous, "a netmask is a run of ones followed by zeros");
    return 0;
}
