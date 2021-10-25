#ifndef __SIHD_NET_TCPCLIENT_HPP__
# define __SIHD_NET_TCPCLIENT_HPP__

# include <sihd/net/Socket.hpp>
# include <sihd/net/IReceiver.hpp>
# include <sihd/net/ISender.hpp>
# include <sihd/util/IRunnable.hpp>
# include <sihd/util/Handler.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Poll.hpp>

namespace sihd::net
{

class TcpClient:    public virtual IReceiver,
                    public virtual ISender,
                    public virtual sihd::util::IRunnable,
                    public virtual sihd::util::IHandler<int>
{
    public:
        TcpClient(bool ipv6 = false);
        TcpClient(const IpAddr & ip);
        TcpClient(const std::string & ip, int port);
        TcpClient(const std::string & path);
        virtual ~TcpClient();

        bool connect(const IpAddr & addr);
        bool connect(const std::string & ip, int port);
        bool connect(const std::string & path) { return _socket.connect(path); }

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        bool close();

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

        ssize_t receive(void *buf, size_t len) { return _socket.receive(buf, len); }
        ssize_t receive(sihd::util::IArray & arr) { return _socket.receive(arr); }
        ssize_t receive();

        ssize_t send(const void *data, size_t len) { return _socket.send(data, len); }
        bool send_all(const void *data, size_t len) { return _socket.send_all(data, len); }

        ssize_t send(const sihd::util::IArray & arr) { return _socket.send(arr); }
        bool send_all(const sihd::util::IArray & arr) { return _socket.send_all(arr); }

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