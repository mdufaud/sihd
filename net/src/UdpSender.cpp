#include <sihd/net/UdpSender.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/NamedFactory.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(UdpSender)

SIHD_LOGGER;

UdpSender::UdpSender(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent) {}

UdpSender::~UdpSender() = default;

bool UdpSender::open_socket_unix()
{
    return _socket.is_open() || _socket.open(AF_UNIX, SOCK_DGRAM, IPPROTO_UDP);
}

bool UdpSender::open_socket(bool ipv6)
{
    return _socket.is_open() || _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

bool UdpSender::connect(const IpAddr & addr)
{
    return _socket.connect(addr);
}

bool UdpSender::open_and_connect(const IpAddr & ip)
{
    return this->open_socket(ip.is_ipv6()) && this->connect(ip);
}

bool UdpSender::open_and_connect(std::string_view ip, int port)
{
    IpAddr addr(ip, port);
    return this->open_socket(addr.is_ipv6()) && this->connect(addr);
}

bool UdpSender::open_unix_and_connect(std::string_view path)
{
    return this->open_socket_unix() && this->connect(path);
}

bool UdpSender::close()
{
    _socket.shutdown();
    return _socket.close();
}

ssize_t UdpSender::send(sihd::util::ArrCharView view)
{
    return _socket.send(view);
}

bool UdpSender::send_all(sihd::util::ArrCharView view)
{
    return _socket.send_all(view);
}

ssize_t UdpSender::send_to(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return _socket.send_to(addr, view);
}

bool UdpSender::send_to_all(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return _socket.send_all_to(addr, view);
}

} // namespace sihd::net