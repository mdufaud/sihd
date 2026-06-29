#include <csignal>

#include <sihd/net/TlsSocket.hpp>
#include <sihd/sys/SigThreadBlocker.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace sihd::net
{

SIHD_LOGGER;

TlsSocket::TlsSocket() {}

TlsSocket::TlsSocket(int socket, bool get_infos): Socket(socket, get_infos) {}

TlsSocket::TlsSocket(TlsSocket && other) noexcept:
    Socket(std::move(other)),
    _tls_ctx(std::move(other._tls_ctx)),
    _tls_conn(std::move(other._tls_conn))
{
}

TlsSocket & TlsSocket::operator=(TlsSocket && other) noexcept
{
    if (this != &other)
    {
        this->close();
        Socket::operator=(std::move(other));
        _tls_ctx = std::move(other._tls_ctx);
        _tls_conn = std::move(other._tls_conn);
    }
    return *this;
}

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

bool TlsSocket::tls_accept(int timeout_ms)
{
    if (!_tls_ctx || !this->is_open())
    {
        SIHD_LOG(error, "TlsSocket: cannot tls_accept without context and open socket");
        return false;
    }
    if (!_tls_conn.init(*_tls_ctx, this->socket()))
        return false;
    if (!_tls_conn.accept(timeout_ms))
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
        if (!_tls_conn.init(*_tls_ctx, this->socket()))
        {
            Socket::close();
            return false;
        }
        if (!_tls_conn.connect(timeout_ms))
        {
            _tls_conn.clear();
            Socket::close();
            return false;
        }
    }
    return true;
}

ssize_t TlsSocket::send(sihd::util::ArrCharView view)
{
    if (_tls_conn)
        return _tls_conn.write(view.data(), view.size());
    return Socket::send(view);
}

ssize_t TlsSocket::receive(void *data, size_t size)
{
    if (_tls_conn)
        return _tls_conn.read(data, size);
    return Socket::receive(data, size);
}

bool TlsSocket::close()
{
    if (_tls_conn)
    {
#if !defined(__SIHD_WINDOWS__)
        sihd::sys::SigThreadBlocker sigpipe_guard(SIGPIPE);
#endif
        _tls_conn.shutdown();
        _tls_conn.clear();
    }
    return Socket::close();
}

} // namespace sihd::net
