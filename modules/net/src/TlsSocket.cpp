#include <cstdio>

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
    SIHD_UNEXPECTED_LOG(this->close());
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
        if (SIHD_UNEXPECTED_LOG(this->set_blocking(false)) || SIHD_UNEXPECTED_LOG(_tls_conn.init(*_tls_ctx, *this)))
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
    return this->_handshake(true, timeout_ms).has_value();
}

std::expected<void, sihd::util::Error> TlsSocket::_handshake(bool is_accept, int timeout_ms)
{
    auto init = _tls_conn.init(*_tls_ctx, *this);
    if (!init)
        return init;
    auto res = is_accept ? _tls_conn.accept(timeout_ms) : _tls_conn.connect(timeout_ms);
    if (!res)
    {
        _tls_conn.clear();
        return res;
    }
    return {};
}

std::expected<void, sihd::util::Error> TlsSocket::connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms)
{
    auto res = Socket::connect(addr, addr_len, timeout_ms);
    if (!res)
        return res;
    if (_tls_ctx)
    {
        auto handshake = this->_handshake(false, timeout_ms);
        if (!handshake)
        {
            (void)Socket::close();
            return handshake;
        }
    }
    return {};
}

std::expected<size_t, sihd::util::Error> TlsSocket::send(sihd::util::ArrCharView view)
{
    if (_tls_conn)
        return _tls_conn.write(view.data(), view.size());
    return Socket::send(view);
}

std::expected<size_t, sihd::util::Error> TlsSocket::receive(void *data, size_t size)
{
    if (_tls_conn)
        return _tls_conn.read(data, size);
    return Socket::receive(data, size);
}

std::expected<void, sihd::util::Error> TlsSocket::shutdown() const
{
    // the TLS close_notify write must happen before SHUT_RDWR, or SSL_shutdown
    // gets EPIPE and the process dies of SIGPIPE
    if (_tls_conn)
        (void)_tls_conn.shutdown();
    return Socket::shutdown();
}

std::expected<void, sihd::util::Error> TlsSocket::close()
{
    if (_tls_conn)
    {
        (void)_tls_conn.shutdown();
        _tls_conn.clear();
    }
    return Socket::close();
}

} // namespace sihd::net
