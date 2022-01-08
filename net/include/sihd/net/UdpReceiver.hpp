#ifndef __SIHD_NET_UDPRECEIVER_HPP__
# define __SIHD_NET_UDPRECEIVER_HPP__

# include <sihd/net/Socket.hpp>
# include <sihd/net/INetReceiver.hpp>

# include <sihd/util/Named.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Handler.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Poll.hpp>
# include <sihd/util/Observable.hpp>

namespace sihd::net
{

class UdpReceiver:  public INetReceiver,
                    public sihd::util::Named,
                    public sihd::util::Configurable,
                    public sihd::util::IStoppableRunnable,
                    public sihd::util::Observable<INetReceiver>,
                    public sihd::util::IHandler<sihd::util::Poll *>
{
    public:
        UdpReceiver(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~UdpReceiver();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();

        bool socket_opened() { return _socket.is_open(); }

        bool bind(const IpAddr & addr);
        bool bind_unix(const std::string & path);

        bool open_and_bind(const IpAddr & ip);
        bool open_and_bind(const std::string & ip, int port);
        bool open_unix_and_bind(const std::string & path);

        bool close();

        ssize_t receive(void *buf, size_t len);
        ssize_t receive(sihd::util::IArray & arr);

        // INetReceiver
        ssize_t receive(IpAddr & addr, void *buf, size_t len);
        ssize_t receive(IpAddr & addr, sihd::util::IArray & arr);

        bool set_poll_timeout(int milliseconds);
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

        // to set blocking/broadcast
        const Socket & socket() const { return _socket; }
        const IpAddr & client_addr() const { return _client_addr; }

    protected:
    
    private:
        void handle(sihd::util::Poll *poll);
        void _setup_poll();

        Socket _socket;
        IpAddr _client_addr;
        std::mutex _poll_mutex;
        sihd::util::Poll _poll;
};

}

#endif 