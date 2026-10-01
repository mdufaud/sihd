#ifndef __SIHD_CRYPTO_UTILS_HPP__
#define __SIHD_CRYPTO_UTILS_HPP__

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::crypto
{

class PrivateKey;
class Certificate;

namespace utils
{

std::expected<std::vector<uint8_t>, sihd::util::Error>
    sign_data(const PrivateKey & key, const uint8_t *data, size_t len);

bool verify_data(const Certificate & cert,
                 const uint8_t *data,
                 size_t data_len,
                 const uint8_t *signature,
                 size_t sig_len);

std::expected<std::vector<uint8_t>, sihd::util::Error> sign_data(const PrivateKey & key, sihd::util::ArrByteView data);

bool verify_data(const Certificate & cert, sihd::util::ArrByteView data, sihd::util::ArrByteView signature);

} // namespace utils
} // namespace sihd::crypto

#endif
