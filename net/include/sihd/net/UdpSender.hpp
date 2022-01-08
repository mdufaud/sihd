#ifndef __SIHD_NET_UDPSENDER_HPP__
# define __SIHD_NET_UDPSENDER_HPP__

# include <sihd/util/Named.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/net/Socket.hpp>
# include <sihd/net/INetSender.hpp>

namespace sihd::net
{

class UdpSender:    public INetSender,
                    public sihd::util::Named,
                    public sihd::util::Configurable
{
    public:
        UdpSender(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~UdpSender();

        bool open_socket(bool ipv6 = false);
        bool open_socket_unix();

        bool socket_opened() { return _socket.is_open(); }

        bool connect(const IpAddr & addr);
        bool connect_unix(const std::string & path) { return _socket.connect_unix(path); }

        bool open_and_connect(const IpAddr & ip);
        bool open_and_connect(const std::string & ip, int port);
        bool open_unix_and_connect(const std::string & path);

        bool close();

        ssize_t send(const void *data, size_t len);
        bool send_all(const void *data, size_t len);

        ssize_t send(const sihd::util::IArray & arr);
        bool send_all(const sihd::util::IArray & arr);

        ssize_t send_to(const IpAddr & addr, const void *data, size_t len);
        bool send_to_all(const IpAddr & addr, const void *data, size_t len);

        ssize_t send_to(const IpAddr & addr, const sihd::util::IArray & arr);
        bool send_to_all(const IpAddr & addr, const sihd::util::IArray & arr);

        const Socket & socket() const { return _socket; }

    protected:
    
    private:
        Socket  _socket;
};

}

#endif 