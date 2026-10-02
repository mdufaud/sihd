#ifndef __SIHD_NET_UDPSENDER_HPP__
#define __SIHD_NET_UDPSENDER_HPP__

#include <expected>

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

        std::expected<void, sihd::util::Error> open_socket(bool ipv6 = false);
        std::expected<void, sihd::util::Error> open_socket_unix();

        bool socket_opened() { return _socket.is_open(); }

        std::expected<void, sihd::util::Error> connect(const IpAddr & addr);
        std::expected<void, sihd::util::Error> connect_unix(std::string_view path);

        std::expected<void, sihd::util::Error> open_and_connect(const IpAddr & ip);
        std::expected<void, sihd::util::Error> open_and_connect(std::string_view ip, int port);
        std::expected<void, sihd::util::Error> open_unix_and_connect(std::string_view path);

        std::expected<void, sihd::util::Error> close();

        std::expected<size_t, sihd::util::Error> send(sihd::util::ArrCharView view);
        std::expected<void, sihd::util::Error> send_all(sihd::util::ArrCharView view);

        std::expected<size_t, sihd::util::Error> send_to(const IpAddr & addr, sihd::util::ArrCharView view);
        std::expected<void, sihd::util::Error> send_to_all(const IpAddr & addr, sihd::util::ArrCharView view);

        const Socket & socket() const { return _socket; }

    protected:

    private:
        Socket _socket;
};

} // namespace sihd::net

#endif