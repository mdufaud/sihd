#include <sihd/net/UdpSender.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(UdpSender)

SIHD_LOGGER;

UdpSender::UdpSender(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent) {}

UdpSender::~UdpSender() = default;

sihd::util::Result<void> UdpSender::open_socket_unix()
{
    if (_socket.is_open())
        return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::already_exists, "socket already open"));
    return _socket.open(AF_UNIX, SOCK_DGRAM, 0);
}

sihd::util::Result<void> UdpSender::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::already_exists, "socket already open"));
    return _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

sihd::util::Result<void> UdpSender::connect(const IpAddr & addr)
{
    return _socket.connect(addr);
}

sihd::util::Result<void> UdpSender::open_and_connect(const IpAddr & ip)
{
    auto opened = this->open_socket(ip.is_ipv6());
    if (!opened)
        return opened;
    return this->connect(ip);
}

sihd::util::Result<void> UdpSender::open_and_connect(std::string_view ip, int port)
{
    IpAddr addr(ip, port);
    auto opened = this->open_socket(addr.is_ipv6());
    if (!opened)
        return opened;
    return this->connect(addr);
}

sihd::util::Result<void> UdpSender::open_unix_and_connect(std::string_view path)
{
    auto opened = this->open_socket_unix();
    if (!opened)
        return opened;
    return this->connect_unix(path);
}

sihd::util::Result<void> UdpSender::close()
{
    (void)_socket.shutdown();
    return _socket.close();
}

sihd::util::Result<size_t> UdpSender::send(sihd::util::ArrCharView view)
{
    return _socket.send(view);
}

sihd::util::Result<void> UdpSender::send_all(sihd::util::ArrCharView view)
{
    return _socket.send_all(view);
}

sihd::util::Result<void> UdpSender::connect_unix(std::string_view path)
{
    return _socket.connect_unix(path);
}

sihd::util::Result<size_t> UdpSender::send_to(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return _socket.send_to(addr, view);
}

sihd::util::Result<void> UdpSender::send_to_all(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return _socket.send_all_to(addr, view);
}

} // namespace sihd::net