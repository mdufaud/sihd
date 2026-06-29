#ifndef __SIHD_NET_TLSCONNECTION_HPP__
#define __SIHD_NET_TLSCONNECTION_HPP__

#include <cstddef>
#include <sys/types.h>

namespace sihd::crypto
{
class TlsContext;
}

namespace sihd::net
{

class TlsConnection
{
    public:
        TlsConnection();
        ~TlsConnection();

        TlsConnection(const TlsConnection &) = delete;
        TlsConnection & operator=(const TlsConnection &) = delete;
        TlsConnection(TlsConnection && other) noexcept;
        TlsConnection & operator=(TlsConnection && other) noexcept;

        operator bool() const { return _handle != nullptr; }

        bool init(sihd::crypto::TlsContext & ctx, int fd);
        bool connect(int timeout_ms = -1);
        bool accept(int timeout_ms = -1);

        ssize_t read(void *buf, size_t len);
        ssize_t write(const void *buf, size_t len);

        bool shutdown();
        void clear();

        void *native() const { return _handle; }

    private:
        void *_handle;
};

} // namespace sihd::net

#endif
