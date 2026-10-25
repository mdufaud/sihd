#ifndef __SIHD_NET_TLSCONNECTION_HPP__
#define __SIHD_NET_TLSCONNECTION_HPP__

#include <cstddef>
#include <memory>
#include <string_view>

#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::crypto
{
class TlsContext;
}

namespace sihd::net
{

class Socket;

enum class TlsHandshakeStep
{
    complete,
    want_read,
    want_write
};

class TlsConnection
{
    public:
        TlsConnection() = default;
        ~TlsConnection() = default;

        // non-movable: the Socket& bound at init must outlive the SSL handle
        TlsConnection(const TlsConnection &) = delete;
        TlsConnection & operator=(const TlsConnection &) = delete;

        operator bool() const;

        sihd::util::Result<void> init(sihd::crypto::TlsContext & ctx, Socket & socket);
        // between init and connect
        sihd::util::Result<void> set_server_name(sihd::util::ZStringView host);
        sihd::util::Result<void> connect(int timeout_ms);
        // null until the client has read the server's tickets; freeing the connection before shutdown() drops it
        std::shared_ptr<void> session() const;
        // before connect; the server may still refuse it
        sihd::util::Result<void> set_session(const std::shared_ptr<void> & session);
        bool session_reused() const;
        sihd::util::Result<void> accept(int timeout_ms);
        sihd::util::Result<TlsHandshakeStep> accept_step();

        // 0 = clean close from peer
        sihd::util::Result<size_t> read(void *buf, size_t len);
        sihd::util::Result<size_t> write(const void *buf, size_t len);

        bool pending() const;

        sihd::util::Result<void> shutdown() const;
        void clear();

        void *native() const;

    private:
        struct SslFree
        {
                void operator()(void *ssl) const;
        };

        // type-erased SSL: the deleter freeing it lives in the source,
        // keeping openssl types out of this header
        std::unique_ptr<void, SslFree> _handle;
        Socket *_socket = nullptr;
};

} // namespace sihd::net

#endif
