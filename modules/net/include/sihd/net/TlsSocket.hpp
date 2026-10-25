#ifndef __SIHD_NET_TLSSOCKET_HPP__
#define __SIHD_NET_TLSSOCKET_HPP__

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/crypto/TlsContext.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/TlsConnection.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

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
        // defaults to the ip connected to
        void set_tls_server_name(std::string name);
        sihd::util::Result<void> tls_accept(int timeout_ms = blocking_timeout);
        sihd::util::Result<TlsHandshakeStep> tls_accept_step();
        bool tls_active() const;
        bool tls_pending() const;

        using Socket::connect;
        using Socket::receive;
        using Socket::send;

        sihd::util::Result<void>
            connect(const sockaddr *addr, socklen_t addr_len, int timeout_ms = blocking_timeout) override;
        sihd::util::Result<size_t> send(sihd::util::ArrCharView view) override;
        sihd::util::Result<size_t> receive(void *data, size_t size) override;
        sihd::util::Result<void> shutdown() const override;
        sihd::util::Result<void> close() override;

    private:
        sihd::util::Result<void> _handshake(bool is_accept, int timeout_ms, sihd::util::ZStringView server_name = "");

        std::optional<sihd::crypto::TlsContext> _tls_ctx;
        std::string _tls_server_name;
        TlsConnection _tls_conn;
};

} // namespace sihd::net

#endif
