#ifndef __SIHD_NET_TCPSERVER_HPP__
#define __SIHD_NET_TCPSERVER_HPP__

#include <expected>

#include <sihd/net/INetServer.hpp>
#include <sihd/net/INetServerHandler.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::net
{

class TcpServer: public INetServer,
                 public sihd::util::ABlockingService,
                 public sihd::util::Named,
                 public sihd::util::Configurable,
                 public sihd::util::IHandler<sihd::sys::Poll *>
{
    public:
        TcpServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~TcpServer();

        std::expected<void, sihd::util::Error> open_socket(bool ipv6 = false);
        std::expected<void, sihd::util::Error> open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        std::expected<void, sihd::util::Error> bind(const IpAddr & addr);
        std::expected<void, sihd::util::Error> bind_unix(std::string_view path) { return _socket.bind_unix(path); }

        std::expected<void, sihd::util::Error> open_and_bind(const IpAddr & ip);
        std::expected<void, sihd::util::Error> open_and_bind(std::string_view ip, int port);
        std::expected<void, sihd::util::Error> open_unix_and_bind(std::string_view path);

        std::expected<void, sihd::util::Error> close();

        // INetServer
        std::expected<int, sihd::util::Error> accept_client(IpAddr *client_ip = nullptr, int timeout_ms = -1) override;
        bool add_client_read(int socket) override;
        bool add_client_write(int socket) override;
        bool remove_client_read(int socket) override;
        bool remove_client_write(int socket) override;

        bool set_queue_size(size_t size);
        bool set_poll_timeout(int milliseconds);
        bool set_poll_limit(int limit);

        void set_server_handler(INetServerHandler *handler);

        // to set blocking/broadcast
        const Socket & socket() const { return _socket; }
        size_t queue_size() const { return _queue_size; }

    protected:
        void handle(sihd::sys::Poll *poll) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        void _setup_poll();

        Socket _socket;
        size_t _queue_size;
        std::mutex _poll_mutex;
        sihd::sys::Poll _poll;
        INetServerHandler *_server_handler_ptr;
};

} // namespace sihd::net

#endif