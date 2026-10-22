#ifndef __SIHD_NET_UTILS_HPP__
#define __SIHD_NET_UTILS_HPP__

#include <cstdint>

namespace sihd::net::utils
{

uint16_t checksum(uint16_t *addr, int len);

} // namespace sihd::net::utils

#endif
