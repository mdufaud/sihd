#ifndef __SIHD_CRYPTO_UTILS_HPP__
#define __SIHD_CRYPTO_UTILS_HPP__

#include <cstddef>
#include <cstdint>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::crypto
{

class PrivateKey;
class Certificate;

namespace utils
{

sihd::util::Result<std::vector<uint8_t>> sign_data(const PrivateKey & key, const uint8_t *data, size_t len);

bool verify_data(const Certificate & cert,
                 const uint8_t *data,
                 size_t data_len,
                 const uint8_t *signature,
                 size_t sig_len);

sihd::util::Result<std::vector<uint8_t>> sign_data(const PrivateKey & key, sihd::util::ArrByteView data);

bool verify_data(const Certificate & cert, sihd::util::ArrByteView data, sihd::util::ArrByteView signature);

} // namespace utils
} // namespace sihd::crypto

#endif
