#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/TlsContext.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/util/Error.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

using sihd::util::Error;
using enum sihd::util::ErrorCode;

namespace sihd::crypto
{

namespace
{

SSL_CTX *as_ctx(void *h)
{
    return static_cast<SSL_CTX *>(h);
}

std::unexpected<sihd::util::Error> error_not_initialized()
{
    return std::unexpected(Error(not_initialized, "TlsContext: context not initialized"));
}

} // namespace

TlsContext::TlsContext(): _handle(nullptr) {}

TlsContext::~TlsContext()
{
    this->clear();
}

TlsContext::TlsContext(const TlsContext & other): _handle(nullptr)
{
    if (other._handle)
    {
        SSL_CTX_up_ref(as_ctx(other._handle));
        _handle = other._handle;
    }
}

TlsContext & TlsContext::operator=(const TlsContext & other)
{
    if (this != &other)
    {
        this->clear();
        if (other._handle)
        {
            SSL_CTX_up_ref(as_ctx(other._handle));
            _handle = other._handle;
        }
    }
    return *this;
}

TlsContext::TlsContext(TlsContext && other) noexcept: _handle(other._handle)
{
    other._handle = nullptr;
}

TlsContext & TlsContext::operator=(TlsContext && other) noexcept
{
    if (this != &other)
    {
        this->clear();
        _handle = other._handle;
        other._handle = nullptr;
    }
    return *this;
}

std::expected<void, sihd::util::Error> TlsContext::init(bool server_mode)
{
    ERR_clear_error();
    this->clear();
    const SSL_METHOD *method = server_mode ? TLS_server_method() : TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx)
        return make_error("TlsContext: SSL_CTX_new");
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    _handle = ctx;
    return {};
}

std::expected<void, sihd::util::Error> TlsContext::set_certificate(const Certificate & cert)
{
    ERR_clear_error();
    if (!_handle)
        return error_not_initialized();
    if (!cert)
        return std::unexpected(Error(invalid_argument, "TlsContext: certificate is empty"));
    if (SSL_CTX_use_certificate(as_ctx(_handle), static_cast<X509 *>(cert.native())) != 1)
        return make_error("TlsContext: SSL_CTX_use_certificate");
    return {};
}

std::expected<void, sihd::util::Error> TlsContext::set_private_key(const PrivateKey & key)
{
    ERR_clear_error();
    if (!_handle)
        return error_not_initialized();
    if (!key)
        return std::unexpected(Error(invalid_argument, "TlsContext: private key is empty"));
    if (SSL_CTX_use_PrivateKey(as_ctx(_handle), static_cast<EVP_PKEY *>(key.native())) != 1)
        return make_error("TlsContext: SSL_CTX_use_PrivateKey");
    return {};
}

std::expected<void, sihd::util::Error> TlsContext::load_ca_cert(std::string_view path)
{
    ERR_clear_error();
    if (!_handle)
        return error_not_initialized();
    std::string p(path);
    if (SSL_CTX_load_verify_locations(as_ctx(_handle), p.c_str(), nullptr) != 1)
        return make_error("TlsContext: load CA cert from '{}'", path);
    return {};
}

void TlsContext::set_verify_peer(bool verify)
{
    if (!_handle)
        return;
    SSL_CTX_set_verify(as_ctx(_handle), verify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);
}

void TlsContext::clear()
{
    if (_handle)
    {
        SSL_CTX_free(as_ctx(_handle));
        _handle = nullptr;
    }
}

} // namespace sihd::crypto
