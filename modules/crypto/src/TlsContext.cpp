#include <fmt/format.h>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/TlsContext.hpp>
#include <sihd/crypto/error.hpp>
#include <sihd/util/Result.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::crypto
{

namespace
{

struct CtxFree
{
        void operator()(SSL_CTX *ctx) const { SSL_CTX_free(ctx); }
};

std::unexpected<Error> error_not_initialized()
{
    return std::unexpected(Error(not_initialized, "context not initialized"));
}

} // namespace

Result<void> TlsContext::init(bool server_mode)
{
    ERR_clear_error();
    this->clear();
    const SSL_METHOD *method = server_mode ? TLS_server_method() : TLS_client_method();
    std::unique_ptr<SSL_CTX, CtxFree> ctx(SSL_CTX_new(method));
    if (ctx == nullptr)
        return make_error("SSL_CTX_new");
    if (SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION) != 1)
        return make_error("SSL_CTX_set_min_proto_version");
    SSL_CTX_set_options(ctx.get(), SSL_OP_NO_RENEGOTIATION);
    if (!server_mode)
    {
        SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);
        if (SSL_CTX_set_default_verify_paths(ctx.get()) != 1)
            return make_error("SSL_CTX_set_default_verify_paths");
    }
    _handle = std::shared_ptr<void>(ctx.release(), CtxFree {});
    _server_mode = server_mode;
    return {};
}

Result<void> TlsContext::set_certificate(const Certificate & cert)
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

Result<void> TlsContext::set_private_key(const PrivateKey & key)
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

Result<void> TlsContext::load_ca_cert(std::string_view path)
{
    ERR_clear_error();
    if (!_handle)
        return error_not_initialized();
    const std::string path_str(path);
    if (SSL_CTX_load_verify_locations(static_cast<SSL_CTX *>(_handle.get()), path_str.c_str(), nullptr) != 1)
        return make_error(fmt::format("load CA cert from '{}'", path));
    return {};
}

Result<void> TlsContext::set_verify_peer(bool verify)
{
    if (!_handle)
        return error_not_initialized();
    int mode = SSL_VERIFY_NONE;
    if (verify)
        mode = _server_mode ? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT : SSL_VERIFY_PEER;
    SSL_CTX_set_verify(static_cast<SSL_CTX *>(_handle.get()), mode, nullptr);
    return {};
}

Result<void> TlsContext::set_session_id_context(std::string_view context)
{
    ERR_clear_error();
    if (!_handle)
        return error_not_initialized();
    if (context.empty())
        return std::unexpected(Error(invalid_argument, "session id context is empty"));
    if (SSL_CTX_set_session_id_context(static_cast<SSL_CTX *>(_handle.get()),
                                       reinterpret_cast<const unsigned char *>(context.data()),
                                       static_cast<unsigned int>(context.size()))
        != 1)
        return make_error("SSL_CTX_set_session_id_context");
    return {};
}

Result<void> TlsContext::set_renegotiation(bool allow)
{
    if (!_handle)
        return error_not_initialized();
    if (allow)
        SSL_CTX_clear_options(static_cast<SSL_CTX *>(_handle.get()), SSL_OP_NO_RENEGOTIATION);
    else
        SSL_CTX_set_options(static_cast<SSL_CTX *>(_handle.get()), SSL_OP_NO_RENEGOTIATION);
    return {};
}

void TlsContext::clear()
{
    _handle.reset();
}

TlsContext::operator bool() const
{
    return _handle != nullptr;
}

void *TlsContext::native() const
{
    return _handle.get();
}

} // namespace sihd::crypto
