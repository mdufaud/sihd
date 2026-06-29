#ifndef __SIHD_CRYPTO_DIGEST_HPP__
#define __SIHD_CRYPTO_DIGEST_HPP__

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>

namespace sihd::crypto::digest
{

std::vector<uint8_t> compute(std::string_view algorithm, const uint8_t *data, size_t len);

std::vector<uint8_t> compute(std::string_view algorithm, sihd::util::ArrByteView data);

} // namespace sihd::crypto::digest

#endif
