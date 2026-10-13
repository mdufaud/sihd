#ifndef __SIHD_CRYPTO_DIGEST_HPP__
#define __SIHD_CRYPTO_DIGEST_HPP__

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::crypto::digest
{

sihd::util::Result<std::vector<uint8_t>> compute(std::string_view algorithm, const uint8_t *data, size_t len);

sihd::util::Result<std::vector<uint8_t>> compute(std::string_view algorithm, sihd::util::ArrByteView data);

} // namespace sihd::crypto::digest

#endif
