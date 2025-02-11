#ifndef __SIHD_NET_TCPSERVER_HPP__
#define __SIHD_NET_TCPSERVER_HPP__

#include <sihd/net/INetServer.hpp>
#include <sihd/net/INetServerHandler.hpp>
#include <sihd/net/Socket.hpp>

#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::net
{

class TcpServer: public INetServer,
                 public sihd::util::Named,
                 public sihd::util::Configurable,
                 public sihd::util::IHandler<sihd::util::Poll *>
{
    public:
        TcpServer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~TcpServer();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        bool bind(const IpAddr & addr);
        bool bind(std::string_view path) { return _socket.bind(path); }

        bool open_and_bind(const IpAddr & ip);
        bool open_and_bind(std::string_view ip, int port);
        bool open_unix_and_bind(std::string_view path);

        bool close();

        // INetServer
        int accept_client(IpAddr *client_ip = nullptr) override;
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
        void handle(sihd::util::Poll *poll) override;
        bool on_start() override;
        bool on_stop() override;

    private:
        void _setup_poll();

        Socket _socket;
        size_t _queue_size;
        std::mutex _poll_mutex;
        sihd::util::Poll _poll;
        INetServerHandler *_server_handler_ptr;
};

} // namespace sihd::net

#endif