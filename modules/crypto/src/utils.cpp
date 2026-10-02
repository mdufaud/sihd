#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/crypto/utils.hpp>
#include <sihd/util/Error.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

using sihd::util::Error;
using enum sihd::util::ErrorCode;
using sihd::util::error_errno;

namespace sihd::crypto
{

namespace
{

sihd::util::ErrorCode errc_from_ssl(unsigned long ssl_code)
{
    const int reason = ERR_GET_REASON(ssl_code);
    // system errors carry the errno as their reason
    if (ERR_GET_LIB(ssl_code) == ERR_LIB_SYS)
        return error_errno(reason);
    switch (ERR_GET_LIB(ssl_code))
    {
        // PEM / DER input data that could not be parsed
        case ERR_LIB_PEM:
        case ERR_LIB_ASN1:
        case ERR_LIB_OSSL_DECODER:
            return invalid_argument;
        default:
            return io_error;
    }
}

} // namespace

std::unexpected<sihd::util::Error> make_error(std::string_view context)
{
    // the queue is a per-thread fifo: stale entries from earlier failures may sit in front,
    // the latest entry is the one from the failing call
    unsigned long ssl_code = 0;
    unsigned long entry = 0;
    while ((entry = ERR_get_error()) != 0)
        ssl_code = entry;
    char text[256] = {};
    ERR_error_string_n(ssl_code, text, sizeof(text));
    if (ssl_code == 0)
        return std::unexpected(Error(unknown, "{}: no OpenSSL error queued", context));
    return std::unexpected(Error(errc_from_ssl(ssl_code), "{}: {}", context, text));
}

} // namespace sihd::crypto

namespace sihd::crypto::utils
{

std::expected<std::vector<uint8_t>, sihd::util::Error>
    sign_data(const PrivateKey & key, const uint8_t *data, size_t len)
{
    ERR_clear_error();
    if (!key)
        return std::unexpected(Error(not_initialized, "cannot sign with an empty private key"));

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
        return std::unexpected(Error(out_of_memory, "could not allocate EVP_MD_CTX"));

    EVP_PKEY *pkey = static_cast<EVP_PKEY *>(key.native());
    if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return make_error("EVP_DigestSignInit");
    }

    if (EVP_DigestSignUpdate(ctx, data, len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return make_error("EVP_DigestSignUpdate");
    }

    size_t sig_len = 0;
    if (EVP_DigestSignFinal(ctx, nullptr, &sig_len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return make_error("EVP_DigestSignFinal");
    }

    std::vector<uint8_t> sig(sig_len);
    if (EVP_DigestSignFinal(ctx, sig.data(), &sig_len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return make_error("EVP_DigestSignFinal");
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
    {
        ERR_clear_error();
        return false;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
    {
        ERR_clear_error();
        return false;
    }

    if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1)
    {
        EVP_MD_CTX_free(ctx);
        ERR_clear_error();
        return false;
    }

    if (EVP_DigestVerifyUpdate(ctx, data, data_len) != 1)
    {
        EVP_MD_CTX_free(ctx);
        ERR_clear_error();
        return false;
    }

    int ret = EVP_DigestVerifyFinal(ctx, signature, sig_len);
    EVP_MD_CTX_free(ctx);
    // a mismatch is a normal outcome: drain it so a later make_error is not misattributed
    if (ret != 1)
        ERR_clear_error();
    return ret == 1;
}

std::expected<std::vector<uint8_t>, sihd::util::Error> sign_data(const PrivateKey & key, sihd::util::ArrByteView data)
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
