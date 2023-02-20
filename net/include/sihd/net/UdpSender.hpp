#ifndef __SIHD_NET_UDPSENDER_HPP__
#define __SIHD_NET_UDPSENDER_HPP__

#include <sihd/net/INetSender.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::net
{

class UdpSender: public INetSender,
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
        bool connect_unix(std::string_view path) { return _socket.connect_unix(path); }

        bool open_and_connect(const IpAddr & ip);
        bool open_unix_and_connect(std::string_view path);

        bool close();

        ssize_t send(sihd::util::ArrCharView view);
        bool send_all(sihd::util::ArrCharView view);

        ssize_t send_to(const IpAddr & addr, sihd::util::ArrCharView view);
        bool send_to_all(const IpAddr & addr, sihd::util::ArrCharView view);

        const Socket & socket() const { return _socket; }

    protected:

    private:
        Socket _socket;
};

} // namespace sihd::net

#endif