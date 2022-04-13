#include <sihd/net/UdpSender.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::net
{

SIHD_UTIL_REGISTER_FACTORY(UdpSender)

SIHD_LOGGER;

UdpSender::UdpSender(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

UdpSender::~UdpSender()
{
}

bool    UdpSender::open_socket_unix()
{
    return _socket.is_open() || _socket.open(AF_UNIX, SOCK_DGRAM, IPPROTO_UDP);
}

bool    UdpSender::open_socket(bool ipv6)
{
    return _socket.is_open() || _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

bool    UdpSender::connect(const IpAddr & addr)
{
    return _socket.connect(addr);
}

bool    UdpSender::open_and_connect(const IpAddr & ip)
{
    return this->open_socket(ip.prefers_ipv6()) && this->connect(ip);
}

bool    UdpSender::open_and_connect(std::string_view ip, int port)
{
    IpAddr addr(ip, port, true);
    return this->open_socket(addr.prefers_ipv6()) && this->connect(addr);
}

bool    UdpSender::open_unix_and_connect(std::string_view path)
{
    return this->open_socket_unix() && this->connect(path);
}

bool    UdpSender::close()
{
    _socket.shutdown();
    return _socket.close();
}

ssize_t UdpSender::send(const void *data, size_t len)
{
    return _socket.send(data, len);
}

bool    UdpSender::send_all(const void *data, size_t len)
{
    return _socket.send_all(data, len);
}

ssize_t UdpSender::send(const sihd::util::IArray & arr)
{
    return _socket.send(arr);
}

bool    UdpSender::send_all(const sihd::util::IArray & arr)
{
    return _socket.send_all(arr);
}

ssize_t UdpSender::send_to(const IpAddr & addr, const void *data, size_t len)
{
    return _socket.send_to(addr, data, len);
}

bool    UdpSender::send_to_all(const IpAddr & addr, const void *data, size_t len)
{
    return _socket.send_all_to(addr, data, len);
}

ssize_t UdpSender::send_to(const IpAddr & addr, const sihd::util::IArray & arr)
{
    return _socket.send_to(addr, arr);
}

bool    UdpSender::send_to_all(const IpAddr & addr, const sihd::util::IArray & arr)
{
    return _socket.send_all_to(addr, arr);
}


}