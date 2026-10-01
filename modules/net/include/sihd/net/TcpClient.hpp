#ifndef __SIHD_NET_TCPCLIENT_HPP__
#define __SIHD_NET_TCPCLIENT_HPP__

#include <atomic>
#include <expected>

#include <sihd/net/INetReceiver.hpp>
#include <sihd/net/INetSender.hpp>
#include <sihd/net/TlsSocket.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::net
{

class TcpClient: public INetReceiver,
                 public INetSender,
                 public sihd::util::Named,
                 public sihd::util::Configurable,
                 public sihd::util::ABlockingService,
                 public sihd::util::Observable<INetReceiver>,
                 public sihd::util::IHandler<sihd::sys::Poll *>
{
    public:
        TcpClient(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~TcpClient();

        std::expected<void, sihd::util::Error> open_socket(bool ipv6 = false);
        std::expected<void, sihd::util::Error> open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        std::expected<void, sihd::util::Error> connect(const IpAddr & addr, int timeout_ms = Socket::blocking_timeout);
        std::expected<void, sihd::util::Error> connect(std::string_view path);
        std::expected<void, sihd::util::Error> reconnect(int timeout_ms = Socket::blocking_timeout);

        std::expected<void, sihd::util::Error> open_and_connect(const IpAddr & ip,
                                                                int timeout_ms = Socket::blocking_timeout);
        std::expected<void, sihd::util::Error>
            open_and_connect(std::string_view ip, int port, int timeout_ms = Socket::blocking_timeout);
        std::expected<void, sihd::util::Error> open_unix_and_connect(std::string_view path);

        bool set_poll_timeout(int milliseconds);
        // bounds each blocking receive (0 = unbounded): a poll-driven drain must never park on a quiet socket
        bool set_recv_timeout(int milliseconds);
        // poll for x milliseconds - returns true if socket is read
        bool poll(int milliseconds);
        // poll once with configured timeout
        bool poll();

        std::expected<size_t, sihd::util::Error> receive(void *buf, size_t len);

        // INetReceiver
        std::expected<void, sihd::util::Error> close() override;
        std::expected<size_t, sihd::util::Error> receive(IpAddr & addr, sihd::util::IArray & arr) override;
        std::expected<size_t, sihd::util::Error> receive(sihd::util::IArray & arr) override;

        // INetSender
        std::expected<size_t, sihd::util::Error> send(sihd::util::ArrCharView view) override;
        std::expected<void, sihd::util::Error> send_all(sihd::util::ArrCharView view) override;

        void set_tls_context(sihd::crypto::TlsContext ctx);

        // to set blocking/broadcast
        const TlsSocket & socket() const { return _socket; }
        bool connected() const { return _connected; }
        const IpAddr & client_addr() const { return _client_addr; }

    protected:
        void handle(sihd::sys::Poll *poll) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        void _setup_poll();

        TlsSocket _socket;
        IpAddr _client_addr;
        std::atomic<bool> _connected;
        std::mutex _poll_mutex;
        sihd::sys::Poll _poll;
        int _recv_timeout = 0;
};

} // namespace sihd::net

#endif