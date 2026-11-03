#ifndef __SIHD_NET_UTILS_HPP__
#define __SIHD_NET_UTILS_HPP__

#include <cstdint>

#include <sihd/util/ArrayView.hpp>

namespace sihd::net::utils
{

// RFC 1071 folded sum of big endian words: an even sized block (a pseudo-header) chains into the next call
uint32_t checksum_add(sihd::util::ArrUByteView data, uint32_t sum = 0);
// written big endian, a block followed by its checksum sums to 0
uint16_t checksum(sihd::util::ArrUByteView data, uint32_t sum = 0);

} // namespace sihd::net::utils

#endif
