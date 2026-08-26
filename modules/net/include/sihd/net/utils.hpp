#ifndef __SIHD_NET_UTILS_HPP__
#define __SIHD_NET_UTILS_HPP__

#include <cstdint>

#if defined(__SIHD_WINDOWS__)
// missing BSD netinet ip/icmp structures on windows
# include <sihd/net/utils.windows.hpp>
#endif

namespace sihd::net::utils
{

uint16_t checksum(uint16_t *addr, int len);

} // namespace sihd::net::utils

#endif
