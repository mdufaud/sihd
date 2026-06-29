#include <sihd/crypto/cipher.hpp>
#include <sihd/util/Logger.hpp>

#include <openssl/evp.h>

namespace sihd::crypto::cipher
{

SIHD_NEW_LOGGER("sihd::crypto::cipher");

namespace
{

std::vector<uint8_t> do_cipher(bool encrypt,
                               std::string_view algorithm,
                               const uint8_t *key,
                               const uint8_t *iv,
                               const uint8_t *data,
                               size_t data_len)
{
    std::string algo(algorithm);
    const EVP_CIPHER *ciph = EVP_CIPHER_fetch(nullptr, algo.c_str(), nullptr);
    if (!ciph)
    {
        SIHD_LOG(error, "cipher: unknown algorithm: {}", algorithm);
        return {};
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        EVP_CIPHER_free(const_cast<EVP_CIPHER *>(ciph));
        return {};
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
        return {};
    }

    std::vector<uint8_t> out(data_len + static_cast<size_t>(EVP_CIPHER_CTX_block_size(ctx)));
    int out_len = 0;
    int final_len = 0;

    if (encrypt)
    {
        ok = EVP_EncryptUpdate(ctx, out.data(), &out_len, data, static_cast<int>(data_len)) == 1
             && EVP_EncryptFinal_ex(ctx, out.data() + out_len, &final_len) == 1;
    }
    else
    {
        ok = EVP_DecryptUpdate(ctx, out.data(), &out_len, data, static_cast<int>(data_len)) == 1
             && EVP_DecryptFinal_ex(ctx, out.data() + out_len, &final_len) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok)
        return {};

    out.resize(static_cast<size_t>(out_len + final_len));
    return out;
}

} // namespace

std::vector<uint8_t> encrypt(std::string_view algorithm,
                             const uint8_t *key,
                             [[maybe_unused]] size_t key_len,
                             const uint8_t *iv,
                             [[maybe_unused]] size_t iv_len,
                             const uint8_t *data,
                             size_t data_len)
{
    return do_cipher(true, algorithm, key, iv, data, data_len);
}

std::vector<uint8_t> decrypt(std::string_view algorithm,
                             const uint8_t *key,
                             [[maybe_unused]] size_t key_len,
                             const uint8_t *iv,
                             [[maybe_unused]] size_t iv_len,
                             const uint8_t *data,
                             size_t data_len)
{
    return do_cipher(false, algorithm, key, iv, data, data_len);
}

std::vector<uint8_t> encrypt(std::string_view algorithm,
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

std::vector<uint8_t> decrypt(std::string_view algorithm,
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
