#include <cstdio>

#include <sihd/net/TlsSocket.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

using namespace sihd::util;

namespace sihd::net
{

TlsSocket::TlsSocket() {}

TlsSocket::TlsSocket(int socket, bool get_infos): Socket(socket, get_infos) {}

TlsSocket::~TlsSocket()
{
    (void)this->close();
}

void TlsSocket::set_tls_context(sihd::crypto::TlsContext ctx)
{
    _tls_ctx = std::move(ctx);
}

void TlsSocket::set_tls_server_name(std::string name)
{
    _tls_server_name = std::move(name);
}

bool TlsSocket::tls_active() const
{
    return static_cast<bool>(_tls_conn);
}

bool TlsSocket::tls_pending() const
{
    return _tls_conn.pending();
}

Result<TlsHandshakeStep> TlsSocket::tls_accept_step()
{
    if (!_tls_ctx || !this->is_open())
        return std::unexpected(Error(ErrorCode::not_initialized, "cannot accept TLS without context and open socket"));

    if (!_tls_conn)
    {
        auto non_blocking = this->set_blocking(false);
        SIHD_UNEXPECTED_RETURN(non_blocking);

        auto init = _tls_conn.init(*_tls_ctx, *this);
        SIHD_UNEXPECTED_RETURN(init);
    }

    auto stepped = _tls_conn.accept_step();
    if (!stepped)
        _tls_conn.clear();
    return stepped;
}

Result<void> TlsSocket::tls_accept(int timeout_ms)
{
    if (!_tls_ctx || !this->is_open())
    {
        return std::unexpected(Error(ErrorCode::not_initialized, "cannot tls_accept without context and open socket"));
    }
    return this->_handshake(true, timeout_ms);
}

Result<void> TlsSocket::_handshake(bool is_accept, int timeout_ms, std::string_view server_name)
{
    auto init = _tls_conn.init(*_tls_ctx, *this);
    if (!init)
        return init;
    if (!is_accept && !server_name.empty())
    {
        auto named = _tls_conn.set_server_name(server_name);
        if (!named)
        {
            _tls_conn.clear();
            return named;
        }
    }
    auto res = is_accept ? _tls_conn.accept(timeout_ms) : _tls_conn.connect(timeout_ms);
    if (!res)
    {
        _tls_conn.clear();
        return res;
    }
    return {};
}

Result<void> TlsSocket::connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms)
{
    auto res = Socket::connect(addr, addr_len, timeout_ms);
    if (!res)
        return res;
    if (_tls_ctx)
    {
        auto handshake = this->_handshake(
            false,
            timeout_ms,
            _tls_server_name.empty() ? ip::to_str(addr) : std::string_view(_tls_server_name));
        if (!handshake)
        {
            (void)Socket::close();
            return handshake;
        }
    }
    return {};
}

Result<size_t> TlsSocket::send(ArrCharView view)
{
    if (_tls_conn)
        return _tls_conn.write(view.data(), view.size());
    return Socket::send(view);
}

Result<size_t> TlsSocket::receive(void *data, size_t size)
{
    if (_tls_conn)
        return _tls_conn.read(data, size);
    return Socket::receive(data, size);
}

Result<void> TlsSocket::shutdown() const
{
    // the TLS close_notify write must happen before SHUT_RDWR
    if (_tls_conn)
        (void)_tls_conn.shutdown();
    return Socket::shutdown();
}

Result<void> TlsSocket::close()
{
    if (_tls_conn)
    {
        (void)_tls_conn.shutdown();
        _tls_conn.clear();
    }
    return Socket::close();
}

} // namespace sihd::net
