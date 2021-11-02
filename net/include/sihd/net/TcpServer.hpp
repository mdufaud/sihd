#ifndef __SIHD_NET_TCPSERVER_HPP__
# define __SIHD_NET_TCPSERVER_HPP__

# include <sihd/net/Socket.hpp>
# include <sihd/net/INetServer.hpp>
# include <sihd/net/INetServerHandler.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/Handler.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Poll.hpp>

namespace sihd::net
{

class TcpServer: public INetServer, public sihd::util::Configurable
{
    public:
        TcpServer(bool ipv6 = false);
        TcpServer(const IpAddr & addr);
        TcpServer(const std::string & ip, int port);
        TcpServer(const std::string & path);
        virtual ~TcpServer();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        bool bind(const IpAddr & addr);
        bool bind(const std::string & path) { return _socket.bind(path); }

        bool close();

        // inetserver
        bool serve(size_t max_co);
        bool stop_serving();
        int accept_client(IpAddr *client_ip = nullptr);
		bool add_client_read(int socket);
		bool add_client_write(int socket);
		bool remove_client_read(int socket);
		bool remove_client_write(int socket);

        // calls infinite polling
        bool run();
        // ends infinite polling
        bool stop();
        bool is_running() const { return _poll.is_running(); }

        bool set_max_connections(size_t maxco);
        bool set_poll_timeout(int milliseconds);

        void set_server_handler(INetServerHandler *handler);

        // to set blocking/broadcast
        const Socket & socket() const { return _socket; }
        // waitable is notified when a packet comes
        sihd::util::Waitable & waitable() { return _waitable; }
        size_t max_connections() const { return _max_connections; }

    protected:
    
    private:
        void _handle_read(int socket);
        void _handle_write(int socket);
        bool _handle_prepoll();
        void _handle_postpoll(time_t nano, bool timed_out);
        void _init();
        void _setup_poll(size_t maxco);

        Socket _socket;
        size_t _max_connections;
        std::mutex _poll_mutex;
        sihd::util::Poll _poll;
        sihd::util::Waitable _waitable;
        INetServerHandler *_server_handler_ptr;
};

}

#endif 