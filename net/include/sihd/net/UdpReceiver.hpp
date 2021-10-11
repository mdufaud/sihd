#ifndef __SIHD_NET_UDPRECEIVER_HPP__
# define __SIHD_NET_UDPRECEIVER_HPP__

# include <sihd/net/Socket.hpp>
# include <sihd/net/IReceiver.hpp>
# include <sihd/util/IRunnable.hpp>
# include <sihd/util/Handler.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Poll.hpp>

namespace sihd::net
{

class UdpReceiver:  public virtual IReceiver,
                    public virtual sihd::util::IRunnable,
                    public virtual sihd::util::IHandler<int>
{
    public:
        UdpReceiver(bool ipv6 = false);
        UdpReceiver(const IpAddr & addr);
        UdpReceiver(const std::string & ip, int port);
        UdpReceiver(const std::string & path);
        virtual ~UdpReceiver();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        bool bind(const IpAddr & addr);
        bool bind(const std::string & path) { return _socket.bind(path); }

        bool close();

        ssize_t receive(void *buf, size_t len) { return _socket.receive(buf, len); }
        ssize_t receive(sihd::util::IArray & arr) { return _socket.receive(arr); }
        ssize_t receive();

        ssize_t receive(IpAddr & addr, void *buf, size_t len) { return _socket.receive_from(addr, buf, len); }
        ssize_t receive(IpAddr & addr, sihd::util::IArray & arr) { return _socket.receive_from(addr, arr); }
        ssize_t receive_from(IpAddr & addr);

        void set_buffer(sihd::util::IArray *buffer);
        bool set_buffer_size(size_t size);
        bool set_poll_timeout(int milliseconds) { _poll_timeout_milliseconds = milliseconds; return true;}
        // poll for x milliseconds - returns true if socket is read
        bool poll(int milliseconds);
        // poll once with configured timeout
        bool poll();
        // calls infinite polling
        bool run();
        // stop polling
        void stop();
        bool is_running() const { return _poll.is_running(); }

        // called anytime poll can read the socket
        void set_handler(sihd::util::IHandler<const void *, size_t> *handler);

        // to set blocking/broadcast
        const Socket & socket() const { return _socket; }
        // waitable is notified when a packet comes
        sihd::util::Waitable & waitable() { return _waitable; }
        // internal buffer
        const sihd::util::IArray *buffer() { return _array_ptr; };

    protected:
    
    private:
        void handle(int socket);
        void _init();
        void _setup_poll();
        void _delete_buffer();

        Socket _socket;
        bool _array_owned;
        int _poll_timeout_milliseconds;
        std::mutex _poll_mutex;
        sihd::util::Poll _poll;
        sihd::util::Waitable _waitable;
        sihd::util::IArray *_array_ptr;
        sihd::util::IHandler<const void *, size_t> *_handler_ptr;
};

}

#endif 