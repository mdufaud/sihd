#ifndef __SIHD_NET_TLSSOCKET_HPP__
#define __SIHD_NET_TLSSOCKET_HPP__

#include <cstddef>
#include <expected>
#include <optional>

#include <sihd/crypto/TlsContext.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/TlsConnection.hpp>

namespace sihd::net
{

class TlsSocket: public Socket
{
    public:
        TlsSocket();
        TlsSocket(int socket, bool get_infos = true);
        ~TlsSocket();

        // non-movable: TlsConnection binds the SSL handle to this socket
        TlsSocket(const TlsSocket &) = delete;
        TlsSocket & operator=(const TlsSocket &) = delete;

        void set_tls_context(sihd::crypto::TlsContext ctx);
        std::expected<void, sihd::util::Error> tls_accept(int timeout_ms = blocking_timeout);
        TlsHandshakeStep tls_accept_step();
        bool tls_active() const;
        bool tls_pending() const;

        using Socket::connect;
        using Socket::receive;
        using Socket::send;

        std::expected<void, sihd::util::Error>
            connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms = blocking_timeout) override;
        // TLS writes go through SSL_write: no MSG_NOSIGNAL to pass, unlike plain sockets - a dead
        // connection raises SIGPIPE, which the process must ignore or handle itself
        std::expected<size_t, sihd::util::Error> send(sihd::util::ArrCharView view) override;
        std::expected<size_t, sihd::util::Error> receive(void *data, size_t size) override;
        std::expected<void, sihd::util::Error> shutdown() const override;
        std::expected<void, sihd::util::Error> close() override;

    private:
        std::expected<void, sihd::util::Error> _handshake(bool is_accept, int timeout_ms);

        std::optional<sihd::crypto::TlsContext> _tls_ctx;
        TlsConnection _tls_conn;
};

} // namespace sihd::net

#endif
