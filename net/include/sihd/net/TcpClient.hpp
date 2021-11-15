#ifndef __SIHD_NET_TCPCLIENT_HPP__
# define __SIHD_NET_TCPCLIENT_HPP__

# include <sihd/net/Socket.hpp>
# include <sihd/net/INetReceiver.hpp>
# include <sihd/net/INetSender.hpp>
# include <sihd/util/IRunnable.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/Handler.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Poll.hpp>
# include <sihd/util/Observable.hpp>

namespace sihd::net
{

class TcpClient:    public INetReceiver,
                    public INetSender,
                    public sihd::util::Configurable,
                    public sihd::util::IStoppableRunnable,
                    public sihd::util::Observable<INetReceiver>,
                    public sihd::util::IHandler<sihd::util::Poll *>
{
    public:
        TcpClient();
        TcpClient(bool ipv6 = false);
        TcpClient(const IpAddr & ip);
        TcpClient(const std::string & ip, int port);
        TcpClient(const std::string & path);
        virtual ~TcpClient();

        bool connect(const IpAddr & addr);
        bool connect(const std::string & ip, int port);
        bool connect(const std::string & path);

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        bool close();

        bool set_poll_timeout(int milliseconds) { _poll_timeout_milliseconds = milliseconds; return true;}
        // poll for x milliseconds - returns true if socket is read
        bool poll(int milliseconds);
        // poll once with configured timeout
        bool poll();
        // calls infinite polling
        bool run();
        // stop polling
        bool stop();
        bool is_running() const { return _poll.is_running(); }

        // called anytime poll can read the socket
        void set_handler(sihd::util::IHandler<INetReceiver *> *handler);

        ssize_t receive(IpAddr & addr, sihd::util::IArray & arr) { return _socket.receive_from(addr, arr); }
        ssize_t receive(sihd::util::IArray & arr) { return _socket.receive(arr); }

        ssize_t receive(void *buf, size_t len) { return _socket.receive(buf, len); }

        ssize_t send(const void *data, size_t len) { return _socket.send(data, len); }
        bool send_all(const void *data, size_t len) { return _socket.send_all(data, len); }

        ssize_t send(const sihd::util::IArray & arr) { return _socket.send(arr); }
        bool send_all(const sihd::util::IArray & arr) { return _socket.send_all(arr); }

        // to set blocking/broadcast
        const Socket & socket() const { return _socket; }

        const IpAddr & client_addr() const { return _client_addr; }

    protected:
    
    private:
        void handle(sihd::util::Poll *poll);
        void _init();
        void _setup_poll();

        Socket _socket;
        IpAddr _client_addr;
        bool _array_owned;
        int _poll_timeout_milliseconds;
        std::mutex _poll_mutex;
        sihd::util::Poll _poll;
};

}

#endif 