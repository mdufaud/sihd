#include <sihd/net/UdpSender.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Result.hpp>

using namespace sihd::util;

namespace sihd::net
{

SIHD_REGISTER_FACTORY(UdpSender)

SIHD_LOGGER;

UdpSender::UdpSender(const std::string & name, Node *parent): Named(name, parent) {}

UdpSender::~UdpSender() = default;

Result<void> UdpSender::open_socket_unix()
{
    if (_socket.is_open())
        return std::unexpected(Error(ErrorCode::already_exists, "socket already open"));
    return _socket.open(AF_UNIX, SOCK_DGRAM, 0);
}

Result<void> UdpSender::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return std::unexpected(Error(ErrorCode::already_exists, "socket already open"));
    return _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

bool UdpSender::socket_opened()
{
    return _socket.is_open();
}

Result<void> UdpSender::connect(const IpAddr & addr)
{
    return _socket.connect(addr);
}

Result<void> UdpSender::open_and_connect(const IpAddr & ip)
{
    auto opened = this->open_socket(ip.is_ipv6());
    if (!opened)
        return opened;
    return this->connect(ip);
}

Result<void> UdpSender::open_and_connect(std::string_view ip, int port)
{
    IpAddr addr(ip, port);
    auto opened = this->open_socket(addr.is_ipv6());
    if (!opened)
        return opened;
    return this->connect(addr);
}

Result<void> UdpSender::open_unix_and_connect(std::string_view path)
{
    auto opened = this->open_socket_unix();
    if (!opened)
        return opened;
    return this->connect_unix(path);
}

Result<void> UdpSender::close()
{
    (void)_socket.shutdown();
    return _socket.close();
}

Result<size_t> UdpSender::send(ArrCharView view)
{
    return _socket.send(view);
}

Result<void> UdpSender::send_all(ArrCharView view)
{
    return _socket.send_all(view);
}

Result<void> UdpSender::connect_unix(std::string_view path)
{
    return _socket.connect_unix(path);
}

Result<size_t> UdpSender::send_to(const IpAddr & addr, ArrCharView view)
{
    return _socket.send_to(addr, view);
}

Result<void> UdpSender::send_to_all(const IpAddr & addr, ArrCharView view)
{
    return _socket.send_all_to(addr, view);
}

const Socket & UdpSender::socket() const
{
    return _socket;
}

} // namespace sihd::net