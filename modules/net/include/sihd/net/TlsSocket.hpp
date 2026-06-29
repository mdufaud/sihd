#ifndef __SIHD_NET_TLSSOCKET_HPP__
#define __SIHD_NET_TLSSOCKET_HPP__

#include <optional>

#include <sihd/net/Socket.hpp>
#include <sihd/net/TlsConnection.hpp>
#include <sihd/crypto/TlsContext.hpp>

namespace sihd::net
{

class TlsSocket: public Socket
{
    public:
        TlsSocket();
        TlsSocket(int socket, bool get_infos = true);
        TlsSocket(TlsSocket && other) noexcept;
        TlsSocket & operator=(TlsSocket && other) noexcept;
        ~TlsSocket();

        TlsSocket(const TlsSocket &) = delete;
        TlsSocket & operator=(const TlsSocket &) = delete;

        void set_tls_context(sihd::crypto::TlsContext ctx);
        bool tls_accept(int timeout_ms = blocking_timeout);
        bool tls_active() const;

        using Socket::connect;
        using Socket::receive;
        using Socket::send;

        bool connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms = blocking_timeout) override;
        ssize_t send(sihd::util::ArrCharView view) override;
        ssize_t receive(void *data, size_t size) override;
        bool close() override;

    private:
        std::optional<sihd::crypto::TlsContext> _tls_ctx;
        TlsConnection _tls_conn;
};

} // namespace sihd::net

#endif
