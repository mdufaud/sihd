#include <sihd/crypto/TlsContext.hpp>
#include <sihd/net/TlsConnection.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <fcntl.h>
#else
# include <winsock2.h>
#endif

#include <chrono>

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

bool set_fd_blocking(int fd, bool blocking)
{
#if !defined(__SIHD_WINDOWS__)
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return false;
    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags) == 0;
#else
    u_long mode = blocking ? 0 : 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#endif
}

bool drive_handshake(SSL *ssl, bool is_connect, int timeout_ms)
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

    int fd = SSL_get_fd(ssl);
    if (fd < 0)
    {
        SIHD_LOG(error, "TlsConnection: no fd for timed handshake");
        return false;
    }

    set_fd_blocking(fd, false);
    auto start = std::chrono::steady_clock::now();
    bool ok = false;
    while (true)
    {
        int ret = is_connect ? SSL_connect(ssl) : SSL_accept(ssl);
        if (ret == 1)
        {
            ok = true;
            break;
        }
        int err = SSL_get_error(ssl, ret);
        bool want_read = (err == SSL_ERROR_WANT_READ);
        bool want_write = (err == SSL_ERROR_WANT_WRITE);
        if (!want_read && !want_write)
        {
            SIHD_LOG(error, "TlsConnection: {} failed: {}", what, err);
            break;
        }
        auto elapsed
            = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        int remaining = timeout_ms - static_cast<int>(elapsed);
        if (remaining <= 0)
        {
            SIHD_LOG(error, "TlsConnection: {} timeout after {}ms", what, timeout_ms);
            break;
        }
        sihd::sys::Poll poll;
        poll.set_limit(1);
        if (want_read)
            poll.set_read_fd(fd);
        else
            poll.set_write_fd(fd);
        poll.poll(remaining);
        if (poll.polling_error())
        {
            SIHD_LOG(error, "TlsConnection: {} poll error", what);
            break;
        }
        if (poll.polling_timeout())
        {
            SIHD_LOG(error, "TlsConnection: {} timeout after {}ms", what, timeout_ms);
            break;
        }
    }
    set_fd_blocking(fd, true);
    return ok;
}

} // namespace

TlsConnection::TlsConnection(): _handle(nullptr) {}

TlsConnection::~TlsConnection()
{
    this->clear();
}

TlsConnection::TlsConnection(TlsConnection && other) noexcept: _handle(other._handle)
{
    other._handle = nullptr;
}

TlsConnection & TlsConnection::operator=(TlsConnection && other) noexcept
{
    if (this != &other)
    {
        this->clear();
        _handle = other._handle;
        other._handle = nullptr;
    }
    return *this;
}

bool TlsConnection::init(sihd::crypto::TlsContext & ctx, int fd)
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
    if (SSL_set_fd(ssl, fd) != 1)
    {
        SIHD_LOG(error, "TlsConnection: failed to set fd");
        SSL_free(ssl);
        return false;
    }
    _handle = ssl;
    return true;
}

bool TlsConnection::connect(int timeout_ms)
{
    if (!_handle)
        return false;
    return drive_handshake(as_ssl(_handle), true, timeout_ms);
}

bool TlsConnection::accept(int timeout_ms)
{
    if (!_handle)
        return false;
    return drive_handshake(as_ssl(_handle), false, timeout_ms);
}

ssize_t TlsConnection::read(void *buf, size_t len)
{
    if (!_handle)
        return -1;
    int ret = SSL_read(as_ssl(_handle), buf, static_cast<int>(len));
    if (ret <= 0)
    {
        int err = SSL_get_error(as_ssl(_handle), ret);
        if (err == SSL_ERROR_ZERO_RETURN)
            return 0;
        return -1;
    }
    return ret;
}

ssize_t TlsConnection::write(const void *buf, size_t len)
{
    if (!_handle)
        return -1;
    int ret = SSL_write(as_ssl(_handle), buf, static_cast<int>(len));
    if (ret <= 0)
        return -1;
    return ret;
}

bool TlsConnection::shutdown()
{
    if (!_handle)
        return false;
    SSL_shutdown(as_ssl(_handle));
    return true;
}

void TlsConnection::clear()
{
    if (_handle)
    {
        SSL_free(as_ssl(_handle));
        _handle = nullptr;
    }
}

} // namespace sihd::net
