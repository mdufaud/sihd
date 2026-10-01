#include <sihd/crypto/digest.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/util/Error.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>

using sihd::util::Error;
using enum sihd::util::ErrorCode;

namespace sihd::crypto::digest
{

namespace
{

std::unexpected<sihd::util::Error> unknown_algorithm(std::string_view algorithm)
{
    // fetch failure queues openssl errors: clear so the next make_error is not misattributed
    ERR_clear_error();
    return std::unexpected(Error(invalid_argument, "unknown digest '{}'", algorithm));
}

} // namespace

std::expected<std::vector<uint8_t>, sihd::util::Error>
    compute(std::string_view algorithm, const uint8_t *data, size_t len)
{
    ERR_clear_error();
    std::string algo(algorithm);
    const EVP_MD *md = EVP_MD_fetch(nullptr, algo.c_str(), nullptr);
    if (!md)
        return unknown_algorithm(algorithm);

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
    {
        EVP_MD_free(const_cast<EVP_MD *>(md));
        return make_error("EVP_MD_CTX_new");
    }

    unsigned int digest_len = 0;
    std::vector<uint8_t> result(static_cast<size_t>(EVP_MD_get_size(md)));

    const char *step = "EVP_DigestInit_ex";
    bool ok = EVP_DigestInit_ex(ctx, md, nullptr) == 1;
    if (ok)
    {
        step = "EVP_DigestUpdate";
        ok = EVP_DigestUpdate(ctx, data, len) == 1;
    }
    if (ok)
    {
        step = "EVP_DigestFinal_ex";
        ok = EVP_DigestFinal_ex(ctx, result.data(), &digest_len) == 1;
    }

    EVP_MD_CTX_free(ctx);
    EVP_MD_free(const_cast<EVP_MD *>(md));

    if (!ok)
        return make_error(step);

    result.resize(digest_len);
    return result;
}

std::expected<std::vector<uint8_t>, sihd::util::Error> compute(std::string_view algorithm, sihd::util::ArrByteView data)
{
    return compute(algorithm, reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

} // namespace sihd::crypto::digest
