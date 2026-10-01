#include <sihd/crypto/cipher.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/util/Error.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>

using sihd::util::Error;
using enum sihd::util::ErrorCode;

namespace sihd::crypto::cipher
{

namespace
{

std::unexpected<sihd::util::Error> unknown_algorithm(std::string_view algorithm)
{
    // fetch failure queues openssl errors: clear so the next make_error is not misattributed
    ERR_clear_error();
    return std::unexpected(Error(invalid_argument, "unknown cipher '{}'", algorithm));
}

std::expected<std::vector<uint8_t>, sihd::util::Error> do_cipher(bool encrypt,
                                                                 std::string_view algorithm,
                                                                 const uint8_t *key,
                                                                 const uint8_t *iv,
                                                                 const uint8_t *data,
                                                                 size_t data_len)
{
    ERR_clear_error();
    std::string algo(algorithm);
    const EVP_CIPHER *ciph = EVP_CIPHER_fetch(nullptr, algo.c_str(), nullptr);
    if (!ciph)
        return unknown_algorithm(algorithm);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        EVP_CIPHER_free(const_cast<EVP_CIPHER *>(ciph));
        return make_error("EVP_CIPHER_CTX_new");
    }

    bool ok;
    if (encrypt)
        ok = EVP_EncryptInit_ex2(ctx, ciph, key, iv, nullptr) == 1;
    else
        ok = EVP_DecryptInit_ex2(ctx, ciph, key, iv, nullptr) == 1;

    EVP_CIPHER_free(const_cast<EVP_CIPHER *>(ciph));

    if (!ok)
    {
        EVP_CIPHER_CTX_free(ctx);
        return make_error(encrypt ? "EVP_EncryptInit_ex2" : "EVP_DecryptInit_ex2");
    }

    std::vector<uint8_t> out(data_len + static_cast<size_t>(EVP_CIPHER_CTX_block_size(ctx)));
    int out_len = 0;
    int final_len = 0;
    bool updated = false;
    bool finalized = false;
    if (encrypt)
    {
        updated = EVP_EncryptUpdate(ctx, out.data(), &out_len, data, static_cast<int>(data_len)) == 1;
        finalized = updated && EVP_EncryptFinal_ex(ctx, out.data() + out_len, &final_len) == 1;
    }
    else
    {
        updated = EVP_DecryptUpdate(ctx, out.data(), &out_len, data, static_cast<int>(data_len)) == 1;
        finalized = updated && EVP_DecryptFinal_ex(ctx, out.data() + out_len, &final_len) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!updated)
        return make_error(encrypt ? "EVP_EncryptUpdate" : "EVP_DecryptUpdate");
    if (!finalized)
        return make_error(encrypt ? "EVP_EncryptFinal_ex" : "EVP_DecryptFinal_ex");

    out.resize(static_cast<size_t>(out_len + final_len));
    return out;
}

} // namespace

std::expected<std::vector<uint8_t>, sihd::util::Error> encrypt(std::string_view algorithm,
                                                               const uint8_t *key,
                                                               [[maybe_unused]] size_t key_len,
                                                               const uint8_t *iv,
                                                               [[maybe_unused]] size_t iv_len,
                                                               const uint8_t *data,
                                                               size_t data_len)
{
    return do_cipher(true, algorithm, key, iv, data, data_len);
}

std::expected<std::vector<uint8_t>, sihd::util::Error> decrypt(std::string_view algorithm,
                                                               const uint8_t *key,
                                                               [[maybe_unused]] size_t key_len,
                                                               const uint8_t *iv,
                                                               [[maybe_unused]] size_t iv_len,
                                                               const uint8_t *data,
                                                               size_t data_len)
{
    return do_cipher(false, algorithm, key, iv, data, data_len);
}

std::expected<std::vector<uint8_t>, sihd::util::Error> encrypt(std::string_view algorithm,
                                                               sihd::util::ArrByteView key,
                                                               sihd::util::ArrByteView iv,
                                                               sihd::util::ArrByteView data)
{
    return encrypt(algorithm,
                   reinterpret_cast<const uint8_t *>(key.data()),
                   key.size(),
                   reinterpret_cast<const uint8_t *>(iv.data()),
                   iv.size(),
                   reinterpret_cast<const uint8_t *>(data.data()),
                   data.size());
}

std::expected<std::vector<uint8_t>, sihd::util::Error> decrypt(std::string_view algorithm,
                                                               sihd::util::ArrByteView key,
                                                               sihd::util::ArrByteView iv,
                                                               sihd::util::ArrByteView data)
{
    return decrypt(algorithm,
                   reinterpret_cast<const uint8_t *>(key.data()),
                   key.size(),
                   reinterpret_cast<const uint8_t *>(iv.data()),
                   iv.size(),
                   reinterpret_cast<const uint8_t *>(data.data()),
                   data.size());
}

} // namespace sihd::crypto::cipher
