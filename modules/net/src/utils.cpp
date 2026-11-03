#include <sihd/net/utils.hpp>

namespace sihd::net::utils
{

using namespace sihd::util;

uint32_t checksum_add(ArrUByteView data, uint32_t sum)
{
    const uint8_t *bytes = data.data();
    const size_t size = data.size();
    uint64_t total = sum;
    for (size_t i = 0; i + 1 < size; i += 2)
        total += static_cast<uint32_t>(bytes[i] << 8 | bytes[i + 1]);
    if (size % 2 != 0)
        total += static_cast<uint32_t>(bytes[size - 1] << 8);
    while (total >> 16 != 0)
        total = (total & 0xffff) + (total >> 16);
    return static_cast<uint32_t>(total);
}

uint16_t checksum(ArrUByteView data, uint32_t sum)
{
    return static_cast<uint16_t>(~checksum_add(data, sum));
}

} // namespace sihd::net::utils
