#ifndef __SIHD_NET_UDPSENDER_HPP__
# define __SIHD_NET_UDPSENDER_HPP__

# include <sihd/net/Socket.hpp>
# include <sihd/net/INetSender.hpp>
# include <sihd/util/Configurable.hpp>

namespace sihd::net
{

class UdpSender: public INetSender, public sihd::util::Configurable
{
    public:
        UdpSender(bool ipv6 = false);
        UdpSender(const IpAddr & ip);
        UdpSender(const std::string & ip, int port);
        UdpSender(const std::string & path);
        virtual ~UdpSender();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();
        bool socket_opened() { return _socket.is_open(); }

        bool connect(const IpAddr & addr);
        bool connect(const std::string & path) { return _socket.connect(path); }

        bool close();

        ssize_t send(const void *data, size_t len) { return _socket.send(data, len); }
        bool send_all(const void *data, size_t len) { return _socket.send_all(data, len); }

        ssize_t send(const sihd::util::IArray & arr) { return _socket.send(arr); }
        bool send_all(const sihd::util::IArray & arr) { return _socket.send_all(arr); }

        ssize_t send_to(const IpAddr & addr, const void *data, size_t len) { return _socket.send_to(addr, data, len); }
        bool send_to_all(const IpAddr & addr, const void *data, size_t len) { return _socket.send_all_to(addr, data, len); }

        ssize_t send_to(const IpAddr & addr, const sihd::util::IArray & arr) { return _socket.send_to(addr, arr); }
        bool send_to_all(const IpAddr & addr, const sihd::util::IArray & arr) { return _socket.send_all_to(addr, arr); }

        const Socket & socket() const { return _socket; }

    protected:
    
    private:
        Socket  _socket;
};

}

#endif 