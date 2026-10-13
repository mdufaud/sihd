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

struct CtxFree
{
        void operator()(SSL_CTX *ctx) const { SSL_CTX_free(ctx); }
};

std::unexpected<sihd::util::Error> error_not_initialized()
{
    return std::unexpected(Error(not_initialized, "context not initialized"));
}

} // namespace

std::expected<void, sihd::util::Error> TlsContext::init(bool server_mode)
{
    ERR_clear_error();
    this->clear();
    const SSL_METHOD *method = server_mode ? TLS_server_method() : TLS_client_method();
    std::unique_ptr<SSL_CTX, CtxFree> ctx(SSL_CTX_new(method));
    if (ctx == nullptr)
        return make_error("SSL_CTX_new");
    SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION);
    _handle = std::shared_ptr<void>(ctx.release(), CtxFree {});
    return {};
}

std::expected<void, sihd::util::Error> TlsContext::set_certificate(const Certificate & cert)
{
    ERR_clear_error();
    if (!_handle)
        return error_not_initialized();
    if (!cert)
        return std::unexpected(Error(invalid_argument, "certificate is empty"));
    if (SSL_CTX_use_certificate(static_cast<SSL_CTX *>(_handle.get()), static_cast<X509 *>(cert.native())) != 1)
        return make_error("SSL_CTX_use_certificate");
    return {};
}

std::expected<void, sihd::util::Error> TlsContext::set_private_key(const PrivateKey & key)
{
    ERR_clear_error();
    if (!_handle)
        return error_not_initialized();
    if (!key)
        return std::unexpected(Error(invalid_argument, "private key is empty"));
    if (SSL_CTX_use_PrivateKey(static_cast<SSL_CTX *>(_handle.get()), static_cast<EVP_PKEY *>(key.native())) != 1)
        return make_error("SSL_CTX_use_PrivateKey");
    return {};
}

std::expected<void, sihd::util::Error> TlsContext::load_ca_cert(std::string_view path)
{
    ERR_clear_error();
    if (!_handle)
        return error_not_initialized();
    std::string p(path);
    if (SSL_CTX_load_verify_locations(static_cast<SSL_CTX *>(_handle.get()), p.c_str(), nullptr) != 1)
        return make_error("load CA cert from '{}'", path);
    return {};
}

void TlsContext::set_verify_peer(bool verify)
{
    if (!_handle)
        return;
    SSL_CTX_set_verify(static_cast<SSL_CTX *>(_handle.get()), verify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);
}

void TlsContext::clear()
{
    _handle.reset();
}

} // namespace sihd::crypto
