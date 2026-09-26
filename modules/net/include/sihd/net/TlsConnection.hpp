#ifndef __SIHD_NET_TLSCONNECTION_HPP__
#define __SIHD_NET_TLSCONNECTION_HPP__

#include <sys/types.h>

#include <cstddef>

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
        TlsConnection();
        ~TlsConnection();

        // non-movable: the Socket& bound at init must outlive the SSL handle
        TlsConnection(const TlsConnection &) = delete;
        TlsConnection & operator=(const TlsConnection &) = delete;

        operator bool() const { return _handle != nullptr; }

        bool init(sihd::crypto::TlsContext & ctx, Socket & socket);
        bool connect(int timeout_ms);
        bool accept(int timeout_ms);
        TlsHandshakeStep accept_step();

        ssize_t read(void *buf, size_t len);
        ssize_t write(const void *buf, size_t len);

        bool retryable() const;
        bool pending() const;

        bool shutdown() const;
        void clear();

        void *native() const { return _handle; }

    private:
        void *_handle;
        Socket *_socket = nullptr;
        bool _retryable = false;
};

} // namespace sihd::net

#endif
