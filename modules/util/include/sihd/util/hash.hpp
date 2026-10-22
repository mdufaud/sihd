#ifndef __SIHD_UTIL_HASH_HPP__
#define __SIHD_UTIL_HASH_HPP__

#include <cstddef>
#include <cstdint>

namespace sihd::util::hash
{

inline constexpr size_t sha1_size = 20;

void sha1(const uint8_t *data, size_t len, uint8_t output[sha1_size]);

} // namespace sihd::util::hash

#endif
