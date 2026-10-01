#ifndef __SIHD_CRYPTO_DIGEST_HPP__
#define __SIHD_CRYPTO_DIGEST_HPP__

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::crypto::digest
{

std::expected<std::vector<uint8_t>, sihd::util::Error>
    compute(std::string_view algorithm, const uint8_t *data, size_t len);

std::expected<std::vector<uint8_t>, sihd::util::Error> compute(std::string_view algorithm,
                                                               sihd::util::ArrByteView data);

} // namespace sihd::crypto::digest

#endif
