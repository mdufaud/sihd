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

bool drive_handshake(SSL *ssl, Socket *socket, bool is_connect, int timeout_ms)
{
    const char *what = is_connect ? "connect" : "accept";
    if (timeout_ms < 0)
    {
        int ret = is_connect ? SSL_connect(ssl) : SSL_accept(ssl);
        if (ret != 1)
        {
            SIHD_LOG(error, "TlsConnection: {} failed: {}", what, SSL_get_error(ssl, ret));
            return false;
        }
        return true;
    }

    // a timed handshake polls a non-blocking socket: restore the mode after
    const bool was_blocking = socket->is_blocking();
    if (was_blocking && socket->set_blocking(false) == false)
    {
        SIHD_LOG(error, "TlsConnection: could not set the socket non-blocking for the timed {}", what);
        return false;
    }
    sihd::util::Defer restore_blocking([&] {
        if (was_blocking)
            socket->set_blocking(true);
    });

    const int fd = SSL_get_fd(ssl);
    if (fd < 0)
    {
        SIHD_LOG(error, "TlsConnection: no fd for timed handshake");
        return false;
    }

    sihd::util::SteadyClock clock;
    const sihd::util::time::UnixTime deadline = clock.now() + sihd::util::time::ms(timeout_ms);
    while (true)
    {
        int ret = is_connect ? SSL_connect(ssl) : SSL_accept(ssl);
        if (ret == 1)
            return true;
        const int err = SSL_get_error(ssl, ret);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
        {
            SIHD_LOG(error, "TlsConnection: {} failed: {}", what, err);
            return false;
        }
        const sihd::util::time::UnixTime remaining = deadline - clock.now().nanoseconds();
        if (remaining <= 0)
        {
            SIHD_LOG(error, "TlsConnection: {} timeout after {}ms", what, timeout_ms);
            return false;
        }
        sihd::sys::Poll poll;
        poll.set_limit(1);
        if (err == SSL_ERROR_WANT_READ)
            poll.set_read_fd(fd);
        else
            poll.set_write_fd(fd);
        poll.poll((int)sihd::util::time::to_ms(remaining));
        if (poll.polling_error())
        {
            SIHD_LOG(error, "TlsConnection: {} poll error", what);
            return false;
        }
    }
}

} // namespace

TlsConnection::TlsConnection(): _handle(nullptr) {}

TlsConnection::~TlsConnection()
{
    this->clear();
}

bool TlsConnection::init(sihd::crypto::TlsContext & ctx, Socket & socket)
{
    this->clear();
    if (!ctx)
    {
        SIHD_LOG(error, "TlsConnection: context is empty");
        return false;
    }
    SSL *ssl = SSL_new(static_cast<SSL_CTX *>(ctx.native()));
    if (!ssl)
    {
        SIHD_LOG(error, "TlsConnection: failed to create SSL");
        return false;
    }
    if (SSL_set_fd(ssl, socket.socket()) != 1)
    {
        SIHD_LOG(error, "TlsConnection: failed to set fd");
        SSL_free(ssl);
        return false;
    }
    _handle = ssl;
    _socket = &socket;
    return true;
}

bool TlsConnection::connect(int timeout_ms)
{
    if (!_handle)
        return false;
    return drive_handshake(as_ssl(_handle), _socket, true, timeout_ms);
}

bool TlsConnection::accept(int timeout_ms)
{
    if (!_handle)
        return false;
    return drive_handshake(as_ssl(_handle), _socket, false, timeout_ms);
}

TlsHandshakeStep TlsConnection::accept_step()
{
    if (!_handle)
        return TlsHandshakeStep::failed;
    SSL *ssl = as_ssl(_handle);
    const int ret = SSL_accept(ssl);
    if (ret == 1)
        return TlsHandshakeStep::complete;
    const int err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ)
        return TlsHandshakeStep::want_read;
    if (err == SSL_ERROR_WANT_WRITE)
        return TlsHandshakeStep::want_write;
    SIHD_LOG(error, "TlsConnection: accept failed: {}", err);
    return TlsHandshakeStep::failed;
}

bool TlsConnection::retryable() const
{
    return _retryable;
}

bool TlsConnection::pending() const
{
    return _handle && SSL_pending(as_ssl(_handle)) > 0;
}

ssize_t TlsConnection::read(void *buf, size_t len)
{
    _retryable = false;
    if (!_handle)
        return -1;
    int ret = SSL_read(as_ssl(_handle), buf, static_cast<int>(len));
    if (ret <= 0)
    {
        const int err = SSL_get_error(as_ssl(_handle), ret);
        _retryable = (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE);
        if (err == SSL_ERROR_ZERO_RETURN)
            return 0;
        return -1;
    }
    return ret;
}

ssize_t TlsConnection::write(const void *buf, size_t len)
{
    _retryable = false;
    if (!_handle)
        return -1;
    int ret = SSL_write(as_ssl(_handle), buf, static_cast<int>(len));
    if (ret <= 0)
    {
        const int err = SSL_get_error(as_ssl(_handle), ret);
        _retryable = (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE);
        return -1;
    }
    return ret;
}

bool TlsConnection::shutdown() const
{
    if (!_handle)
        return false;
    SSL_shutdown(as_ssl(_handle));
    return true;
}

void TlsConnection::clear()
{
    _retryable = false;
    if (_handle)
    {
        SSL_free(as_ssl(_handle));
        _handle = nullptr;
    }
    _socket = nullptr;
}

} // namespace sihd::net
