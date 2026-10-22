#ifndef __SIHD_NET_UDPSENDER_HPP__
#define __SIHD_NET_UDPSENDER_HPP__

#include <sihd/net/INetSender.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

class UdpSender: public INetSender,
                 public sihd::util::Named,
                 public sihd::util::Configurable
{
    public:
        UdpSender(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~UdpSender();

        sihd::util::Result<void> open_socket(bool ipv6 = false);
        sihd::util::Result<void> open_socket_unix();

        bool socket_opened();

        sihd::util::Result<void> connect(const IpAddr & addr);
        sihd::util::Result<void> connect_unix(std::string_view path);

        sihd::util::Result<void> open_and_connect(const IpAddr & ip);
        sihd::util::Result<void> open_and_connect(std::string_view ip, int port);
        sihd::util::Result<void> open_unix_and_connect(std::string_view path);

        sihd::util::Result<void> close();

        sihd::util::Result<size_t> send(sihd::util::ArrCharView view);
        sihd::util::Result<void> send_all(sihd::util::ArrCharView view);

        sihd::util::Result<size_t> send_to(const IpAddr & addr, sihd::util::ArrCharView view);
        sihd::util::Result<void> send_to_all(const IpAddr & addr, sihd::util::ArrCharView view);

        const Socket & socket() const;

    protected:

    private:
        Socket _socket;
};

} // namespace sihd::net

#endif