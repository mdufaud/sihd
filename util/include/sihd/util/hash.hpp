#ifndef __SIHD_UTIL_HASH_HPP__
#define __SIHD_UTIL_HASH_HPP__

#include <cstddef>
#include <cstdint>

namespace sihd::util::hash
{

void sha1(const uint8_t *data, size_t len, uint8_t output[20]);

} // namespace sihd::util::hash

#endif
