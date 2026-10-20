#include <cstdio>

#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Result.hpp>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

using namespace sihd::util;
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

int refuse_passphrase(char *, int, int, void *)
{
    return -1;
}

std::shared_ptr<void> make_handle(EVP_PKEY *key)
{
    return std::shared_ptr<void>(key, PkeyFree {});
}

} // namespace

Result<void> PrivateKey::generate_rsa(int bits)
{
    ERR_clear_error();
    this->clear();
    std::unique_ptr<EVP_PKEY_CTX, PkeyCtxFree> ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
    if (ctx == nullptr)
        return make_error("EVP_PKEY_CTX_new_id");
    EVP_PKEY *raw = nullptr;
    const bool ok = EVP_PKEY_keygen_init(ctx.get()) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), bits) > 0
                    && EVP_PKEY_keygen(ctx.get(), &raw) > 0;
    std::unique_ptr<EVP_PKEY, PkeyFree> key(raw);
    if (!ok || key == nullptr)
        return make_error("RSA keygen");
    _handle = make_handle(key.release());
    return {};
}

Result<void> PrivateKey::generate_ec(std::string_view curve_name)
{
    ERR_clear_error();
    this->clear();
    std::unique_ptr<EVP_PKEY_CTX, PkeyCtxFree> ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr));
    if (ctx == nullptr)
        return make_error("EVP_PKEY_CTX_new_from_name");
    // valid char * modifiable
    std::string curve(curve_name);
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("group", curve.data(), 0),
        OSSL_PARAM_END,
    };
    EVP_PKEY *raw = nullptr;
    const bool ok = EVP_PKEY_keygen_init(ctx.get()) > 0 && EVP_PKEY_CTX_set_params(ctx.get(), params) > 0
                    && EVP_PKEY_keygen(ctx.get(), &raw) > 0;
    std::unique_ptr<EVP_PKEY, PkeyFree> key(raw);
    if (!ok || key == nullptr)
        return make_error("EC keygen for curve '{}'", curve_name);
    _handle = make_handle(key.release());
    return {};
}

Result<void> PrivateKey::load_pem(std::string_view path)
{
    ERR_clear_error();
    this->clear();
    FILE *fp = fopen(std::string(path).c_str(), "r");
    if (!fp)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    std::unique_ptr<EVP_PKEY, PkeyFree> key(PEM_read_PrivateKey(fp, nullptr, refuse_passphrase, nullptr));
    fclose(fp);
    if (key == nullptr)
        return make_error("read PEM from '{}'", path);
    _handle = make_handle(key.release());
    return {};
}

Result<void> PrivateKey::save_pem(std::string_view path) const
{
    ERR_clear_error();
    if (!_handle)
        return std::unexpected(Error(not_initialized, "no key loaded"));

    const std::string pem = this->to_pem_string();
    if (pem.empty())
        return make_error("write PEM to '{}'", path);

    // owner-only temp renamed over the target: links are replaced, never written through
    sihd::sys::File file;
    auto opened = file.open_tmp(std::string(path) + ".", true);
    SIHD_UNEXPECTED_RETURN(opened);
    const std::string tmp_path = file.path();
    auto wrote = file.write(pem.data(), pem.size());
    auto closed = file.close();

    Result<void> saved;
    if (!wrote)
        saved = std::unexpected(std::move(wrote).error());
    else if (*wrote != pem.size())
        saved = std::unexpected(Error(io_error, "short write to '{}'", tmp_path));
    else if (!closed)
        saved = std::unexpected(std::move(closed).error());
    else
        saved = sihd::sys::fs::rename(tmp_path, std::string(path));

    if (!saved)
        (void)sihd::sys::fs::remove_file(tmp_path);
    return saved;
}

Result<void> PrivateKey::load_pem_string(std::string_view pem)
{
    ERR_clear_error();
    this->clear();
    BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio)
        return make_error("BIO_new_mem_buf");
    std::unique_ptr<EVP_PKEY, PkeyFree> key(PEM_read_bio_PrivateKey(bio, nullptr, refuse_passphrase, nullptr));
    BIO_free(bio);
    if (key == nullptr)
        return make_error("read PEM from string");
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
