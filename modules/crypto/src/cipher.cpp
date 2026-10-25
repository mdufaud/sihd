#include <climits>
#include <memory>

#include <sihd/crypto/cipher.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/util/Result.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::crypto::cipher
{

namespace
{

std::unexpected<Error> unknown_algorithm(std::string_view algorithm)
{
    // fetch failure queues openssl errors: clear so the next make_error is not misattributed
    ERR_clear_error();
    return std::unexpected(Error(invalid_argument, "unknown cipher '{}'", algorithm));
}

struct CiphFree
{
        void operator()(const EVP_CIPHER *ciph) const { EVP_CIPHER_free(const_cast<EVP_CIPHER *>(ciph)); }
};

struct CtxFree
{
        void operator()(EVP_CIPHER_CTX *ctx) const { EVP_CIPHER_CTX_free(ctx); }
};

Result<std::vector<uint8_t>> do_cipher(bool encrypt,
                                       ZStringView algorithm,
                                       const uint8_t *key,
                                       size_t key_len,
                                       const uint8_t *iv,
                                       size_t iv_len,
                                       const uint8_t *data,
                                       size_t data_len)
{
    ERR_clear_error();
    std::unique_ptr<const EVP_CIPHER, CiphFree> ciph(EVP_CIPHER_fetch(nullptr, algorithm.c_str(), nullptr));
    if (ciph == nullptr)
        return unknown_algorithm(algorithm);

    const int key_size = EVP_CIPHER_get_key_length(ciph.get());
    const int iv_size = EVP_CIPHER_get_iv_length(ciph.get());

    if (key_size <= 0)
        return std::unexpected(Error(invalid_argument, "cipher '{}' does not encrypt", algorithm));
    if (key == nullptr)
        return std::unexpected(Error(invalid_argument, "cipher '{}' needs a key", algorithm));
    if (key_len != static_cast<size_t>(key_size))
        return std::unexpected(
            Error(invalid_argument, "cipher '{}' needs a {} bytes key, got {}", algorithm, key_size, key_len));

    if (iv_size > 0 && iv == nullptr)
        return std::unexpected(Error(invalid_argument, "cipher '{}' needs an iv", algorithm));
    if (iv_size > 0 && iv_len != static_cast<size_t>(iv_size))
        return std::unexpected(
            Error(invalid_argument, "cipher '{}' needs a {} bytes iv, got {}", algorithm, iv_size, iv_len));

    if (data_len > static_cast<size_t>(INT_MAX))
        return std::unexpected(Error(invalid_argument, "cipher input is too large: {} bytes", data_len));

    std::unique_ptr<EVP_CIPHER_CTX, CtxFree> ctx(EVP_CIPHER_CTX_new());
    if (ctx == nullptr)
        return make_error("EVP_CIPHER_CTX_new");

    bool ok;
    if (encrypt)
        ok = EVP_EncryptInit_ex2(ctx.get(), ciph.get(), key, iv, nullptr) == 1;
    else
        ok = EVP_DecryptInit_ex2(ctx.get(), ciph.get(), key, iv, nullptr) == 1;

    if (!ok)
        return make_error(encrypt ? "EVP_EncryptInit_ex2" : "EVP_DecryptInit_ex2");

    std::vector<uint8_t> out(data_len + static_cast<size_t>(EVP_CIPHER_CTX_block_size(ctx.get())));
    int out_len = 0;
    int final_len = 0;
    bool updated = false;
    bool finalized = false;
    if (encrypt)
    {
        updated = EVP_EncryptUpdate(ctx.get(), out.data(), &out_len, data, static_cast<int>(data_len)) == 1;
        finalized = updated && EVP_EncryptFinal_ex(ctx.get(), out.data() + out_len, &final_len) == 1;
    }
    else
    {
        updated = EVP_DecryptUpdate(ctx.get(), out.data(), &out_len, data, static_cast<int>(data_len)) == 1;
        finalized = updated && EVP_DecryptFinal_ex(ctx.get(), out.data() + out_len, &final_len) == 1;
    }

    if (!updated)
        return make_error(encrypt ? "EVP_EncryptUpdate" : "EVP_DecryptUpdate");
    if (!finalized)
        return make_error(encrypt ? "EVP_EncryptFinal_ex" : "EVP_DecryptFinal_ex");

    out.resize(static_cast<size_t>(out_len + final_len));
    return out;
}

} // namespace

Result<std::vector<uint8_t>> encrypt(ZStringView algorithm,
                                     const uint8_t *key,
                                     size_t key_len,
                                     const uint8_t *iv,
                                     size_t iv_len,
                                     const uint8_t *data,
                                     size_t data_len)
{
    return do_cipher(true, algorithm, key, key_len, iv, iv_len, data, data_len);
}

Result<std::vector<uint8_t>> decrypt(ZStringView algorithm,
                                     const uint8_t *key,
                                     size_t key_len,
                                     const uint8_t *iv,
                                     size_t iv_len,
                                     const uint8_t *data,
                                     size_t data_len)
{
    return do_cipher(false, algorithm, key, key_len, iv, iv_len, data, data_len);
}

Result<std::vector<uint8_t>> encrypt(ZStringView algorithm, ArrByteView key, ArrByteView iv, ArrByteView data)
{
    return encrypt(algorithm,
                   reinterpret_cast<const uint8_t *>(key.data()),
                   key.size(),
                   reinterpret_cast<const uint8_t *>(iv.data()),
                   iv.size(),
                   reinterpret_cast<const uint8_t *>(data.data()),
                   data.size());
}

Result<std::vector<uint8_t>> decrypt(ZStringView algorithm, ArrByteView key, ArrByteView iv, ArrByteView data)
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
