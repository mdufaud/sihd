#include <sihd/crypto/utils.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/Certificate.hpp>
#include <sihd/util/Logger.hpp>

#include <openssl/evp.h>
#include <openssl/x509.h>

namespace sihd::crypto::utils
{

SIHD_NEW_LOGGER("sihd::crypto::utils");

std::vector<uint8_t> sign_data(const PrivateKey & key, const uint8_t *data, size_t len)
{
    if (!key)
        return {};

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
        return {};

    EVP_PKEY *pkey = static_cast<EVP_PKEY *>(key.native());
    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    if (EVP_DigestSignUpdate(ctx, data, len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    size_t sig_len = 0;
    if (EVP_DigestSignFinal(ctx, nullptr, &sig_len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    std::vector<uint8_t> sig(sig_len);
    if (EVP_DigestSignFinal(ctx, sig.data(), &sig_len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    EVP_MD_CTX_free(ctx);
    sig.resize(sig_len);
    return sig;
}

bool verify_data(const Certificate & cert,
                 const uint8_t *data,
                 size_t data_len,
                 const uint8_t *signature,
                 size_t sig_len)
{
    if (!cert)
        return false;

    X509 *x509 = static_cast<X509 *>(cert.native());
    EVP_PKEY *pkey = X509_get0_pubkey(x509);
    if (!pkey)
        return false;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
        return false;

    if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    if (EVP_DigestVerifyUpdate(ctx, data, data_len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return false;
    }

    int ret = EVP_DigestVerifyFinal(ctx, signature, sig_len);
    EVP_MD_CTX_free(ctx);
    return ret == 1;
}

std::vector<uint8_t> sign_data(const PrivateKey & key, sihd::util::ArrByteView data)
{
    return sign_data(key, reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

bool verify_data(const Certificate & cert, sihd::util::ArrByteView data, sihd::util::ArrByteView signature)
{
    return verify_data(cert,
                       reinterpret_cast<const uint8_t *>(data.data()),
                       data.size(),
                       reinterpret_cast<const uint8_t *>(signature.data()),
                       signature.size());
}

} // namespace sihd::crypto::utils
