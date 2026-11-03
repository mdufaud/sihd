#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fuzz_helper.hpp>
#include <vector>

#include <sihd/net/utils.hpp>

#include <fuzzer/FuzzedDataProvider.h>

using namespace sihd::net;
using namespace sihd::util;

namespace
{

uint16_t rfc1071_checksum(const std::vector<uint8_t> & bytes)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < bytes.size(); ++i)
        sum += i % 2 == 0 ? static_cast<uint32_t>(bytes[i] << 8) : bytes[i];
    while ((sum >> 16) != 0)
        sum = (sum & 0xffff) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    const size_t split = provider.ConsumeIntegralInRange<size_t>(0, 4096) & ~size_t {1};
    // exact size heap copy: ASan sees a read past the last byte
    const std::vector<uint8_t> bytes = provider.ConsumeRemainingBytes<uint8_t>();
    const uint16_t sum = utils::checksum(ArrUByteView(bytes.data(), bytes.size()));
    test::fuzz_check(sum == rfc1071_checksum(bytes), "the checksum is the rfc 1071 checksum");

    const size_t head = std::min(split, bytes.size() & ~size_t {1});
    const uint32_t partial = utils::checksum_add(ArrUByteView(bytes.data(), head));
    test::fuzz_check(utils::checksum(ArrUByteView(bytes.data() + head, bytes.size() - head), partial) == sum,
                     "an even sized head chains into the rest");

    std::vector<uint8_t> verified(bytes.begin(), bytes.begin() + static_cast<ptrdiff_t>(bytes.size() & ~size_t {1}));
    const uint16_t appended = utils::checksum(ArrUByteView(verified.data(), verified.size()));
    verified.push_back(static_cast<uint8_t>(appended >> 8));
    verified.push_back(static_cast<uint8_t>(appended));
    test::fuzz_check(utils::checksum(ArrUByteView(verified.data(), verified.size())) == 0,
                     "a buffer followed by its checksum verifies to zero");
    return 0;
}
