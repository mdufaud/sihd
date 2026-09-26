#include <sihd/net/TlsSocket.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace sihd::net
{

SIHD_LOGGER;

TlsSocket::TlsSocket() {}

TlsSocket::TlsSocket(int socket, bool get_infos): Socket(socket, get_infos) {}

TlsSocket::~TlsSocket()
{
    this->close();
}

void TlsSocket::set_tls_context(sihd::crypto::TlsContext ctx)
{
    _tls_ctx = std::move(ctx);
}

bool TlsSocket::tls_active() const
{
    return static_cast<bool>(_tls_conn);
}

bool TlsSocket::tls_pending() const
{
    return _tls_conn.pending();
}

TlsHandshakeStep TlsSocket::tls_accept_step()
{
    if (!_tls_ctx || !this->is_open())
    {
        SIHD_LOG(error, "TlsSocket: cannot accept TLS without context and open socket");
        return TlsHandshakeStep::failed;
    }
    if (!_tls_conn)
    {
        if (!this->set_blocking(false) || !_tls_conn.init(*_tls_ctx, *this))
            return TlsHandshakeStep::failed;
    }
    const TlsHandshakeStep step = _tls_conn.accept_step();
    if (step == TlsHandshakeStep::failed)
        _tls_conn.clear();
    return step;
}

bool TlsSocket::tls_accept(int timeout_ms)
{
    if (!_tls_ctx || !this->is_open())
    {
        SIHD_LOG(error, "TlsSocket: cannot tls_accept without context and open socket");
        return false;
    }
    return this->_handshake(true, timeout_ms);
}

bool TlsSocket::_handshake(bool is_accept, int timeout_ms)
{
    if (!_tls_conn.init(*_tls_ctx, *this))
        return false;
    const bool ret = is_accept ? _tls_conn.accept(timeout_ms) : _tls_conn.connect(timeout_ms);
    if (!ret)
    {
        _tls_conn.clear();
        return false;
    }
    return true;
}

bool TlsSocket::connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms)
{
    if (!Socket::connect(addr, addr_len, timeout_ms))
        return false;
    if (_tls_ctx)
    {
        if (!this->_handshake(false, timeout_ms))
        {
            Socket::close();
            return false;
        }
    }
    return true;
}

ssize_t TlsSocket::send(sihd::util::ArrCharView view)
{
    if (_tls_conn)
    {
        ssize_t ret = _tls_conn.write(view.data(), view.size());
        _retryable = ret < 0 && _tls_conn.retryable();
        return ret;
    }
    return Socket::send(view);
}

ssize_t TlsSocket::receive(void *data, size_t size)
{
    if (_tls_conn)
    {
        ssize_t ret = _tls_conn.read(data, size);
        _retryable = ret < 0 && _tls_conn.retryable();
        return ret;
    }
    return Socket::receive(data, size);
}

bool TlsSocket::shutdown() const
{
    // the TLS close_notify write must happen before SHUT_RDWR, or SSL_shutdown
    // gets EPIPE and the process dies of SIGPIPE
    if (_tls_conn)
        _tls_conn.shutdown();
    return Socket::shutdown();
}

bool TlsSocket::close()
{
    if (_tls_conn)
    {
        _tls_conn.shutdown();
        _tls_conn.clear();
    }
    return Socket::close();
}

} // namespace sihd::net
