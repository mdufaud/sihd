#include <sihd/crypto/TlsContext.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/Certificate.hpp>
#include <sihd/util/Logger.hpp>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

namespace sihd::crypto
{

SIHD_LOGGER;

namespace
{

SSL_CTX *as_ctx(void *h) { return static_cast<SSL_CTX *>(h); }

} // namespace

TlsContext::TlsContext(): _handle(nullptr) {}

TlsContext::~TlsContext() { this->clear(); }

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

bool TlsContext::init(bool server_mode)
{
    this->clear();
    const SSL_METHOD *method = server_mode ? TLS_server_method() : TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx)
    {
        SIHD_LOG(error, "TlsContext: failed to create SSL_CTX");
        return false;
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    _handle = ctx;
    return true;
}

bool TlsContext::set_certificate(const Certificate & cert)
{
    if (!_handle || !cert)
        return false;
    return SSL_CTX_use_certificate(as_ctx(_handle), static_cast<X509 *>(cert.native())) == 1;
}

bool TlsContext::set_private_key(const PrivateKey & key)
{
    if (!_handle || !key)
        return false;
    return SSL_CTX_use_PrivateKey(as_ctx(_handle), static_cast<EVP_PKEY *>(key.native())) == 1;
}

bool TlsContext::load_ca_cert(std::string_view path)
{
    if (!_handle)
        return false;
    std::string p(path);
    return SSL_CTX_load_verify_locations(as_ctx(_handle), p.c_str(), nullptr) == 1;
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
