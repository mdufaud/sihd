#include <algorithm>
#include <climits>
#include <csignal>

#include <sihd/crypto/TlsContext.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/TlsConnection.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/SigThreadBlocker.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/time.hpp>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

using namespace sihd::util;

namespace sihd::net
{

namespace
{

struct SessionFree
{
        void operator()(void *session) const { SSL_SESSION_free(static_cast<SSL_SESSION *>(session)); }
};

SSL *as_ssl(void *h)
{
    return static_cast<SSL *>(h);
}

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_EMSCRIPTEN__)
// SSL_read/SSL_write/SSL_shutdown cannot take MSG_NOSIGNAL: a SIGPIPE raised inside stays pending and is drained
struct SigpipeGuard
{
        sihd::sys::SigThreadBlocker blocker {SIGPIPE};
};
#else
struct SigpipeGuard
{
};
#endif

int handshake_once(SSL *ssl, bool is_connect)
{
    return is_connect ? SSL_connect(ssl) : SSL_accept(ssl);
}

bool wants_io(int err)
{
    return err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE;
}

Result<void> handshake_untimed(SSL *ssl, const char *what, bool is_connect)
{
    const int ret = handshake_once(ssl, is_connect);
    if (ret == 1)
        return {};
    const int err = SSL_get_error(ssl, ret);
    if (wants_io(err))
        return std::unexpected(Error(ErrorCode::would_block, "{} would block", what));
    return std::unexpected(Error(ErrorCode::io_error, "{} failed: {}", what, err));
}

Result<void> wait_handshake_io(int fd, int err, time::UnixTime remaining, const char *what)
{
    sihd::sys::Poll poll;
    poll.set_limit(1);
    if (err == SSL_ERROR_WANT_READ)
        poll.set_read_fd(fd);
    else
        poll.set_write_fd(fd);
    poll.poll((int)time::to_ms(remaining));
    if (poll.polling_error())
        return std::unexpected(Error(ErrorCode::io_error, "{} poll error", what));
    return {};
}

// a timed handshake polls a non-blocking socket: restore the mode after
Result<void> handshake_timed(SSL *ssl, Socket *socket, const char *what, bool is_connect, int timeout_ms)
{
    const int fd = SSL_get_fd(ssl);
    if (fd < 0)
        return std::unexpected(Error(ErrorCode::not_initialized, "no fd for timed {}", what));

    auto blocking = socket->is_blocking();
    SIHD_UNEXPECTED_RETURN_CTX(blocking, "timed {}", what);
    const bool was_blocking = *blocking;
    if (was_blocking)
    {
        auto non_blocking = socket->set_blocking(false);
        SIHD_UNEXPECTED_RETURN_CTX(non_blocking, "could not set the socket non-blocking for the timed {}", what);
    }
    Defer restore_blocking([&] {
        if (was_blocking)
            (void)socket->set_blocking(true);
    });

    SteadyClock clock;
    const time::UnixTime deadline = clock.now() + time::ms(timeout_ms);
    while (true)
    {
        const int ret = handshake_once(ssl, is_connect);
        if (ret == 1)
            return {};
        const int err = SSL_get_error(ssl, ret);
        if (!wants_io(err))
            return std::unexpected(Error(ErrorCode::io_error, "{} failed: {}", what, err));
        const time::UnixTime remaining = deadline - clock.now().nanoseconds();
        if (remaining <= 0)
            return std::unexpected(Error(ErrorCode::timeout, "{} timeout after {}ms", what, timeout_ms));
        auto waited = wait_handshake_io(fd, err, remaining, what);
        SIHD_UNEXPECTED_RETURN(waited);
    }
}

Result<void> drive_handshake(SSL *ssl, Socket *socket, bool is_connect, int timeout_ms)
{
    const char *what = is_connect ? "connect" : "accept";
    if (timeout_ms < 0)
        return handshake_untimed(ssl, what, is_connect);
    return handshake_timed(ssl, socket, what, is_connect, timeout_ms);
}

} // namespace

void TlsConnection::SslFree::operator()(void *ssl) const
{
    SSL_free(static_cast<SSL *>(ssl));
}

Result<void> TlsConnection::init(sihd::crypto::TlsContext & ctx, Socket & socket)
{
    this->clear();
    if (!ctx)
        return std::unexpected(Error(ErrorCode::not_initialized, "context is empty"));
    SSL *ssl = SSL_new(static_cast<SSL_CTX *>(ctx.native()));
    if (!ssl)
        return std::unexpected(Error(ErrorCode::unknown, "failed to create SSL"));
    if (SSL_set_fd(ssl, socket.socket()) != 1)
    {
        SSL_free(ssl);
        return std::unexpected(Error(ErrorCode::io_error, "failed to set fd"));
    }
    _handle.reset(ssl);
    _socket = &socket;
    return {};
}

Result<void> TlsConnection::set_server_name(std::string_view host)
{
    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "no TLS connection"));
    if (host.empty() || host.find('\0') != std::string_view::npos)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid server name"));
    ERR_clear_error();
    SSL *ssl = as_ssl(_handle.get());
    const std::string name(host);
    if (sihd::net::ip::is_valid_ip(name))
    {
        if (X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(ssl), name.c_str()) != 1)
            return std::unexpected(Error(ErrorCode::io_error, "could not set the peer ip to verify: {}", name));
        return {};
    }
    if (SSL_set_tlsext_host_name(ssl, name.c_str()) != 1 || SSL_set1_host(ssl, name.c_str()) != 1)
        return std::unexpected(Error(ErrorCode::io_error, "could not set the server name: {}", name));
    return {};
}

std::shared_ptr<void> TlsConnection::session() const
{
    if (!_handle)
        return nullptr;
    SSL_SESSION *session = SSL_get1_session(as_ssl(_handle.get()));
    if (session == nullptr)
        return nullptr;
    if (SSL_SESSION_is_resumable(session) != 1)
    {
        SSL_SESSION_free(session);
        return nullptr;
    }
    return std::shared_ptr<void>(session, SessionFree {});
}

Result<void> TlsConnection::set_session(const std::shared_ptr<void> & session)
{
    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "no TLS connection"));
    if (!session)
        return std::unexpected(Error(ErrorCode::invalid_argument, "session is empty"));
    ERR_clear_error();
    if (SSL_set_session(as_ssl(_handle.get()), static_cast<SSL_SESSION *>(session.get())) != 1)
        return std::unexpected(Error(ErrorCode::io_error, "could not resume the session"));
    return {};
}

bool TlsConnection::session_reused() const
{
    return _handle != nullptr && SSL_session_reused(as_ssl(_handle.get())) == 1;
}

Result<void> TlsConnection::connect(int timeout_ms)
{
    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "no TLS connection"));
    return drive_handshake(as_ssl(_handle.get()), _socket, true, timeout_ms);
}

Result<void> TlsConnection::accept(int timeout_ms)
{
    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "no TLS connection"));
    return drive_handshake(as_ssl(_handle.get()), _socket, false, timeout_ms);
}

Result<TlsHandshakeStep> TlsConnection::accept_step()
{
    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "no TLS connection"));
    SSL *ssl = as_ssl(_handle.get());
    const int ret = SSL_accept(ssl);
    if (ret == 1)
        return TlsHandshakeStep::complete;
    const int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ)
        return TlsHandshakeStep::want_read;
    if (err == SSL_ERROR_WANT_WRITE)
        return TlsHandshakeStep::want_write;
    return std::unexpected(Error(ErrorCode::io_error, "accept failed: {}", err));
}

bool TlsConnection::pending() const
{
    return _handle && SSL_pending(as_ssl(_handle.get())) > 0;
}

Result<size_t> TlsConnection::read(void *buf, size_t len)
{
    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "no TLS connection"));
    [[maybe_unused]] SigpipeGuard sigpipe_guard;
    ERR_clear_error();
    int ret = SSL_read(as_ssl(_handle.get()), buf, static_cast<int>(std::min<size_t>(len, INT_MAX)));
    if (ret <= 0)
    {
        const int err = SSL_get_error(as_ssl(_handle.get()), ret);
        if (err == SSL_ERROR_ZERO_RETURN)
            return (size_t)0;
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            return std::unexpected(Error(ErrorCode::would_block, "read would block"));
        return std::unexpected(Error(ErrorCode::closed, "read failed: {}", err));
    }
    return (size_t)ret;
}

Result<size_t> TlsConnection::write(const void *buf, size_t len)
{
    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "no TLS connection"));
    [[maybe_unused]] SigpipeGuard sigpipe_guard;
    ERR_clear_error();
    int ret = SSL_write(as_ssl(_handle.get()), buf, static_cast<int>(std::min<size_t>(len, INT_MAX)));
    if (ret <= 0)
    {
        const int err = SSL_get_error(as_ssl(_handle.get()), ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            return std::unexpected(Error(ErrorCode::would_block, "write would block"));
        return std::unexpected(Error(ErrorCode::closed, "write failed: {}", err));
    }
    return (size_t)ret;
}

Result<void> TlsConnection::shutdown() const
{
    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "no TLS connection"));
    [[maybe_unused]] SigpipeGuard sigpipe_guard;
    ERR_clear_error();
    if (SSL_is_init_finished(as_ssl(_handle.get())))
        SSL_shutdown(as_ssl(_handle.get()));
    return {};
}

void TlsConnection::clear()
{
    _handle.reset();
    _socket = nullptr;
}

} // namespace sihd::net
