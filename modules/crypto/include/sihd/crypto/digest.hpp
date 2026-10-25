#ifndef __SIHD_CRYPTO_DIGEST_HPP__
#define __SIHD_CRYPTO_DIGEST_HPP__

#include <cstddef>
#include <cstdint>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::crypto::digest
{

sihd::util::Result<std::vector<uint8_t>> compute(sihd::util::ZStringView algorithm, const uint8_t *data, size_t len);

sihd::util::Result<std::vector<uint8_t>> compute(sihd::util::ZStringView algorithm, sihd::util::ArrByteView data);

} // namespace sihd::crypto::digest

#endif
