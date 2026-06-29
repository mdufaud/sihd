#include <sihd/crypto/digest.hpp>
#include <sihd/util/Logger.hpp>

#include <openssl/evp.h>

namespace sihd::crypto::digest
{

SIHD_NEW_LOGGER("sihd::crypto::digest");

std::vector<uint8_t> compute(std::string_view algorithm, const uint8_t *data, size_t len)
{
    std::string algo(algorithm);
    const EVP_MD *md = EVP_MD_fetch(nullptr, algo.c_str(), nullptr);
    if (!md)
    {
        SIHD_LOG(error, "digest: unknown algorithm: {}", algorithm);
        return {};
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
    {
        EVP_MD_free(const_cast<EVP_MD *>(md));
        return {};
    }

    unsigned int digest_len = 0;
    std::vector<uint8_t> result(static_cast<size_t>(EVP_MD_get_size(md)));

    bool ok = EVP_DigestInit_ex(ctx, md, nullptr) == 1
              && EVP_DigestUpdate(ctx, data, len) == 1
              && EVP_DigestFinal_ex(ctx, result.data(), &digest_len) == 1;

    EVP_MD_CTX_free(ctx);
    EVP_MD_free(const_cast<EVP_MD *>(md));

    if (!ok)
        return {};

    result.resize(digest_len);
    return result;
}

std::vector<uint8_t> compute(std::string_view algorithm, sihd::util::ArrByteView data)
{
    return compute(algorithm, reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

} // namespace sihd::crypto::digest
