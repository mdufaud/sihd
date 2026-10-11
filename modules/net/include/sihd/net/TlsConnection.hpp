#ifndef __SIHD_NET_TLSCONNECTION_HPP__
#define __SIHD_NET_TLSCONNECTION_HPP__

#include <sys/types.h>

#include <cstddef>
#include <expected>
#include <memory>

#include <sihd/util/Error.hpp>

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
    want_write,
    failed
};

class TlsConnection
{
    public:
        TlsConnection() = default;
        ~TlsConnection() = default;

        // non-movable: the Socket& bound at init must outlive the SSL handle
        TlsConnection(const TlsConnection &) = delete;
        TlsConnection & operator=(const TlsConnection &) = delete;

        operator bool() const { return _handle != nullptr; }

        std::expected<void, sihd::util::Error> init(sihd::crypto::TlsContext & ctx, Socket & socket);
        std::expected<void, sihd::util::Error> connect(int timeout_ms);
        std::expected<void, sihd::util::Error> accept(int timeout_ms);
        std::expected<TlsHandshakeStep, sihd::util::Error> accept_step();

        // 0 = clean close from peer
        std::expected<size_t, sihd::util::Error> read(void *buf, size_t len);
        std::expected<size_t, sihd::util::Error> write(const void *buf, size_t len);

        bool pending() const;

        std::expected<void, sihd::util::Error> shutdown() const;
        void clear();

        void *native() const { return _handle.get(); }

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
