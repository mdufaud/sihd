#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/util/Logger.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

namespace sihd::crypto
{

SIHD_NEW_LOGGER("sihd::crypto");

namespace
{

EVP_PKEY *as_key(void *h)
{
    return static_cast<EVP_PKEY *>(h);
}

} // namespace

PrivateKey::PrivateKey(): _handle(nullptr) {}

PrivateKey::~PrivateKey()
{
    this->clear();
}

PrivateKey::PrivateKey(const PrivateKey & other): _handle(nullptr)
{
    if (other._handle)
    {
        EVP_PKEY_up_ref(as_key(other._handle));
        _handle = other._handle;
    }
}

PrivateKey & PrivateKey::operator=(const PrivateKey & other)
{
    if (this != &other)
    {
        this->clear();
        if (other._handle)
        {
            EVP_PKEY_up_ref(as_key(other._handle));
            _handle = other._handle;
        }
    }
    return *this;
}

PrivateKey::PrivateKey(PrivateKey && other) noexcept: _handle(other._handle)
{
    other._handle = nullptr;
}

PrivateKey & PrivateKey::operator=(PrivateKey && other) noexcept
{
    if (this != &other)
    {
        this->clear();
        _handle = other._handle;
        other._handle = nullptr;
    }
    return *this;
}

bool PrivateKey::generate_rsa(int bits)
{
    this->clear();
    EVP_PKEY *key = nullptr;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx)
    {
        SIHD_LOG(error, "PrivateKey: failed to create RSA context");
        return false;
    }
    bool ok = EVP_PKEY_keygen_init(ctx) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) > 0
              && EVP_PKEY_keygen(ctx, &key) > 0;
    EVP_PKEY_CTX_free(ctx);
    if (!ok || !key)
    {
        SIHD_LOG(error, "PrivateKey: RSA key generation failed");
        return false;
    }
    _handle = key;
    return true;
}

bool PrivateKey::generate_ec(std::string_view curve_name)
{
    this->clear();
    EVP_PKEY *key = nullptr;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
    if (!ctx)
    {
        SIHD_LOG(error, "PrivateKey: failed to create EC context");
        return false;
    }
    // valid char * modifiable
    std::string curve(curve_name);
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string("group", curve.data(), 0),
        OSSL_PARAM_END,
    };
    bool ok = EVP_PKEY_keygen_init(ctx) > 0 && EVP_PKEY_CTX_set_params(ctx, params) > 0
              && EVP_PKEY_keygen(ctx, &key) > 0;
    EVP_PKEY_CTX_free(ctx);
    if (!ok || !key)
    {
        SIHD_LOG(error, "PrivateKey: EC key generation failed for curve: {}", curve_name);
        return false;
    }
    _handle = key;
    return true;
}

bool PrivateKey::load_pem(std::string_view path)
{
    this->clear();
    FILE *fp = fopen(std::string(path).c_str(), "r");
    if (!fp)
    {
        SIHD_LOG(error, "PrivateKey: cannot open file: {}", path);
        return false;
    }
    EVP_PKEY *key = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    if (!key)
    {
        SIHD_LOG(error, "PrivateKey: failed to read PEM from: {}", path);
        return false;
    }
    _handle = key;
    return true;
}

bool PrivateKey::save_pem(std::string_view path) const
{
    if (!_handle)
        return false;
    FILE *fp = fopen(std::string(path).c_str(), "w");
    if (!fp)
    {
        SIHD_LOG(error, "PrivateKey: cannot open file for writing: {}", path);
        return false;
    }
    bool ok = PEM_write_PrivateKey(fp, as_key(_handle), nullptr, nullptr, 0, nullptr, nullptr) > 0;
    fclose(fp);
    return ok;
}

bool PrivateKey::load_pem_string(std::string_view pem)
{
    this->clear();
    BIO *bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio)
        return false;
    EVP_PKEY *key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!key)
    {
        SIHD_LOG(error, "PrivateKey: failed to read PEM from string");
        return false;
    }
    _handle = key;
    return true;
}

std::string PrivateKey::to_pem_string() const
{
    if (!_handle)
        return {};
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio)
        return {};
    if (PEM_write_bio_PrivateKey(bio, as_key(_handle), nullptr, nullptr, 0, nullptr, nullptr) <= 0)
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
    if (_handle)
    {
        EVP_PKEY_free(as_key(_handle));
        _handle = nullptr;
    }
}

} // namespace sihd::crypto
