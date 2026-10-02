#include <sihd/crypto/TlsContext.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/TlsConnection.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include <openssl/ssl.h>

namespace sihd::net
{

SIHD_LOGGER;

namespace
{

SSL *as_ssl(void *h)
{
    return static_cast<SSL *>(h);
}

std::expected<void, sihd::util::Error> drive_handshake(SSL *ssl, Socket *socket, bool is_connect, int timeout_ms)
{
    using sihd::util::Error;
    using sihd::util::ErrorCode;

    const char *what = is_connect ? "connect" : "accept";
    if (timeout_ms < 0)
    {
        int ret = is_connect ? SSL_connect(ssl) : SSL_accept(ssl);
        if (ret != 1)
        {
            const int err = SSL_get_error(ssl, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                return std::unexpected(Error(ErrorCode::would_block, "TlsConnection: {} would block", what));
            return std::unexpected(Error(ErrorCode::io_error, "TlsConnection: {} failed: {}", what, err));
        }
        return {};
    }

    // a timed handshake polls a non-blocking socket: restore the mode after
    const bool was_blocking = socket->is_blocking();
    if (was_blocking)
    {
        auto blocking = socket->set_blocking(false);
        if (!blocking)
            return std::unexpected(Error(ErrorCode::io_error,
                                         "TlsConnection: could not set the socket non-blocking for the timed {}",
                                         what));
    }
    sihd::util::Defer restore_blocking([&] {
        if (was_blocking)
            (void)socket->set_blocking(true);
    });

    const int fd = SSL_get_fd(ssl);
    if (fd < 0)
        return std::unexpected(Error(ErrorCode::not_initialized, "TlsConnection: no fd for timed {}", what));

    sihd::util::SteadyClock clock;
    const sihd::util::time::UnixTime deadline = clock.now() + sihd::util::time::ms(timeout_ms);
    while (true)
    {
        int ret = is_connect ? SSL_connect(ssl) : SSL_accept(ssl);
        if (ret == 1)
            return {};
        const int err = SSL_get_error(ssl, ret);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
            return std::unexpected(Error(ErrorCode::io_error, "TlsConnection: {} failed: {}", what, err));
        const sihd::util::time::UnixTime remaining = deadline - clock.now().nanoseconds();
        if (remaining <= 0)
            return std::unexpected(Error(ErrorCode::timeout, "TlsConnection: {} timeout after {}ms", what, timeout_ms));
        sihd::sys::Poll poll;
        poll.set_limit(1);
        if (err == SSL_ERROR_WANT_READ)
            poll.set_read_fd(fd);
        else
            poll.set_write_fd(fd);
        poll.poll((int)sihd::util::time::to_ms(remaining));
        if (poll.polling_error())
            return std::unexpected(Error(ErrorCode::io_error, "TlsConnection: {} poll error", what));
    }
}

} // namespace

TlsConnection::TlsConnection(): _handle(nullptr) {}

TlsConnection::~TlsConnection()
{
    this->clear();
}

std::expected<void, sihd::util::Error> TlsConnection::init(sihd::crypto::TlsContext & ctx, Socket & socket)
{
    using sihd::util::Error;
    using sihd::util::ErrorCode;

    this->clear();
    if (!ctx)
        return std::unexpected(Error(ErrorCode::not_initialized, "TlsConnection: context is empty"));
    SSL *ssl = SSL_new(static_cast<SSL_CTX *>(ctx.native()));
    if (!ssl)
        return std::unexpected(Error(ErrorCode::unknown, "TlsConnection: failed to create SSL"));
    if (SSL_set_fd(ssl, socket.socket()) != 1)
    {
        SSL_free(ssl);
        return std::unexpected(Error(ErrorCode::io_error, "TlsConnection: failed to set fd"));
    }
    _handle = ssl;
    _socket = &socket;
    return {};
}

std::expected<void, sihd::util::Error> TlsConnection::connect(int timeout_ms)
{
    if (!_handle)
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::not_initialized, "TlsConnection: no TLS connection"));
    return drive_handshake(as_ssl(_handle), _socket, true, timeout_ms);
}

std::expected<void, sihd::util::Error> TlsConnection::accept(int timeout_ms)
{
    if (!_handle)
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::not_initialized, "TlsConnection: no TLS connection"));
    return drive_handshake(as_ssl(_handle), _socket, false, timeout_ms);
}

std::expected<TlsHandshakeStep, sihd::util::Error> TlsConnection::accept_step()
{
    if (!_handle)
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::not_initialized, "TlsConnection: no TLS connection"));
    SSL *ssl = as_ssl(_handle);
    const int ret = SSL_accept(ssl);
    if (ret == 1)
        return TlsHandshakeStep::complete;
    const int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ)
        return TlsHandshakeStep::want_read;
    if (err == SSL_ERROR_WANT_WRITE)
        return TlsHandshakeStep::want_write;
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::io_error, "TlsConnection: accept failed: {}", err));
}

bool TlsConnection::pending() const
{
    return _handle && SSL_pending(as_ssl(_handle)) > 0;
}

std::expected<size_t, sihd::util::Error> TlsConnection::read(void *buf, size_t len)
{
    using sihd::util::Error;
    using sihd::util::ErrorCode;

    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "TlsConnection: no TLS connection"));
    int ret = SSL_read(as_ssl(_handle), buf, static_cast<int>(len));
    if (ret <= 0)
    {
        const int err = SSL_get_error(as_ssl(_handle), ret);
        if (err == SSL_ERROR_ZERO_RETURN)
            return (size_t)0;
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            return std::unexpected(Error(ErrorCode::would_block, "TlsConnection: read would block"));
        return std::unexpected(Error(ErrorCode::closed, "TlsConnection: read failed: {}", err));
    }
    return (size_t)ret;
}

std::expected<size_t, sihd::util::Error> TlsConnection::write(const void *buf, size_t len)
{
    using sihd::util::Error;
    using sihd::util::ErrorCode;

    if (!_handle)
        return std::unexpected(Error(ErrorCode::not_initialized, "TlsConnection: no TLS connection"));
    int ret = SSL_write(as_ssl(_handle), buf, static_cast<int>(len));
    if (ret <= 0)
    {
        const int err = SSL_get_error(as_ssl(_handle), ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            return std::unexpected(Error(ErrorCode::would_block, "TlsConnection: write would block"));
        return std::unexpected(Error(ErrorCode::closed, "TlsConnection: write failed: {}", err));
    }
    return (size_t)ret;
}

std::expected<void, sihd::util::Error> TlsConnection::shutdown() const
{
    if (!_handle)
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::not_initialized, "TlsConnection: no TLS connection"));
    SSL_shutdown(as_ssl(_handle));
    return {};
}

void TlsConnection::clear()
{
    if (_handle)
    {
        SSL_free(as_ssl(_handle));
        _handle = nullptr;
    }
    _socket = nullptr;
}

} // namespace sihd::net
