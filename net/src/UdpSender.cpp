#include <sihd/net/UdpSender.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

LOGGER;

UdpSender::UdpSender(bool ipv6)
{
    this->open_socket(ipv6);
}

UdpSender::UdpSender(const IpAddr & addr)
{
    if (this->open_socket(addr.prefer_ipv6()))
        this->connect(addr);
}

UdpSender::UdpSender(const std::string & ip, int port)
{
    IpAddr addr(ip, port, true);
    if (this->open_socket(addr.prefer_ipv6()))
        this->connect(addr);
}

UdpSender::UdpSender(const std::string & path)
{
    if (this->open_socket_unix())
        this->connect(path);
}

UdpSender::~UdpSender()
{
}

bool    UdpSender::connect(const IpAddr & addr)
{
    return _socket.connect(addr);
}

bool    UdpSender::close()
{
    _socket.shutdown();
    return _socket.close();
}

bool    UdpSender::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_DGRAM, IPPROTO_UDP);
}

bool    UdpSender::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    return _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

}