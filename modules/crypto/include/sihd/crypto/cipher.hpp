#ifndef __SIHD_CRYPTO_CIPHER_HPP__
#define __SIHD_CRYPTO_CIPHER_HPP__

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>

namespace sihd::crypto::cipher
{

std::vector<uint8_t> encrypt(std::string_view algorithm,
                             const uint8_t *key,
                             size_t key_len,
                             const uint8_t *iv,
                             size_t iv_len,
                             const uint8_t *data,
                             size_t data_len);

std::vector<uint8_t> decrypt(std::string_view algorithm,
                             const uint8_t *key,
                             size_t key_len,
                             const uint8_t *iv,
                             size_t iv_len,
                             const uint8_t *data,
                             size_t data_len);

std::vector<uint8_t> encrypt(std::string_view algorithm,
                             sihd::util::ArrByteView key,
                             sihd::util::ArrByteView iv,
                             sihd::util::ArrByteView data);

std::vector<uint8_t> decrypt(std::string_view algorithm,
                             sihd::util::ArrByteView key,
                             sihd::util::ArrByteView iv,
                             sihd::util::ArrByteView data);

} // namespace sihd::crypto::cipher

#endif
