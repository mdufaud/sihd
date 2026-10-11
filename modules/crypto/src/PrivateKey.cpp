#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/util/Error.hpp>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

using sihd::util::Error;
using enum sihd::util::ErrorCode;

namespace sihd::crypto
{

namespace
{

struct PkeyFree
{
        void operator()(EVP_PKEY *key) const { EVP_PKEY_free(key); }
};

struct PkeyCtxFree
{
        void operator()(EVP_PKEY_CTX *ctx) const { EVP_PKEY_CTX_free(ctx); }
};

std::shared_ptr<void> make_handle(EVP_PKEY *key)
{
    return std::shared_ptr<void>(key, PkeyFree {});
}

} // namespace

std::expected<void, sihd::util::Error> PrivateKey::generate_rsa(int bits)
{
    ERR_clear_error();
    this->clear();
    std::unique_ptr<EVP_PKEY_CTX, PkeyCtxFree> ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
    if (ctx == nullptr)
        return make_error("PrivateKey: EVP_PKEY_CTX_new_id");
    std::unique_ptr<EVP_PKEY, PkeyFree> key;
    const bool ok = EVP_PKEY_keygen_init(ctx.get()) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), bits) > 0
                    && EVP_PKEY_keygen(ctx.get(), std::out_ptr(key)) > 0;
    if (!ok || key == nullptr)
        return make_error("PrivateKey: RSA keygen");
    _handle = make_handle(key.release());
    return {};
}

std::expected<void, sihd::util::Error> PrivateKey::generate_ec(std::string_view curve_name)
{
    ERR_clear_error();
    this->clear();
    std::unique_ptr<EVP_PKEY_CTX, PkeyCtxFree> ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr));
    if (ctx == nullptr)
        return make_error("PrivateKey: EVP_PKEY_CTX_new_from_name");
    // valid char * modifiable
    std::string curve(curve_name);
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("group", curve.data(), 0),
        OSSL_PARAM_END,
    };
    std::unique_ptr<EVP_PKEY, PkeyFree> key;
    const bool ok = EVP_PKEY_keygen_init(ctx.get()) > 0 && EVP_PKEY_CTX_set_params(ctx.get(), params) > 0
                    && EVP_PKEY_keygen(ctx.get(), std::out_ptr(key)) > 0;
    if (!ok || key == nullptr)
        return make_error("PrivateKey: EC keygen for curve '{}'", curve_name);
    _handle = make_handle(key.release());
    return {};
}

std::expected<void, sihd::util::Error> PrivateKey::load_pem(std::string_view path)
{
    ERR_clear_error();
    this->clear();
    FILE *fp = fopen(std::string(path).c_str(), "r");
    if (!fp)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    std::unique_ptr<EVP_PKEY, PkeyFree> key(PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr));
    fclose(fp);
    if (key == nullptr)
        return make_error("PrivateKey: read PEM from '{}'", path);
    _handle = make_handle(key.release());
    return {};
}

std::expected<void, sihd::util::Error> PrivateKey::save_pem(std::string_view path) const
{
    ERR_clear_error();
    if (!_handle)
        return std::unexpected(Error(not_initialized, "PrivateKey: no key loaded"));
    FILE *fp = fopen(std::string(path).c_str(), "w");
    if (!fp)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    const bool
        ok = PEM_write_PrivateKey(fp, static_cast<EVP_PKEY *>(_handle.get()), nullptr, nullptr, 0, nullptr, nullptr)
             > 0;
    fclose(fp);
    if (!ok)
        return make_error("PrivateKey: write PEM to '{}'", path);
    return {};
}

std::expected<void, sihd::util::Error> PrivateKey::load_pem_string(std::string_view pem)
{
    ERR_clear_error();
    this->clear();
    BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio)
        return make_error("PrivateKey: BIO_new_mem_buf");
    std::unique_ptr<EVP_PKEY, PkeyFree> key(PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr));
    BIO_free(bio);
    if (key == nullptr)
        return make_error("PrivateKey: read PEM from string");
    _handle = make_handle(key.release());
    return {};
}

std::string PrivateKey::to_pem_string() const
{
    if (!_handle)
        return {};
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
        return {};
    if (PEM_write_bio_PrivateKey(bio, static_cast<EVP_PKEY *>(_handle.get()), nullptr, nullptr, 0, nullptr, nullptr)
        <= 0)
    {
        BIO_free(bio);
        return {};
    }
    char *data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string result(data, static_cast<size_t>(len));
    BIO_free(bio);
    return result;
}

void PrivateKey::clear()
{
    _handle.reset();
}

} // namespace sihd::crypto
