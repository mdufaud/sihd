#include <sihd/net/UdpSender.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(UdpSender)

SIHD_LOGGER;

UdpSender::UdpSender(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent) {}

UdpSender::~UdpSender() = default;

std::expected<void, sihd::util::Error> UdpSender::open_socket_unix()
{
    if (_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::already_exists, "UdpSender: socket already open"));
    return _socket.open(AF_UNIX, SOCK_DGRAM, 0);
}

std::expected<void, sihd::util::Error> UdpSender::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::already_exists, "UdpSender: socket already open"));
    return _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

std::expected<void, sihd::util::Error> UdpSender::connect(const IpAddr & addr)
{
    return _socket.connect(addr);
}

std::expected<void, sihd::util::Error> UdpSender::open_and_connect(const IpAddr & ip)
{
    auto opened = this->open_socket(ip.is_ipv6());
    if (!opened)
        return opened;
    return this->connect(ip);
}

std::expected<void, sihd::util::Error> UdpSender::open_and_connect(std::string_view ip, int port)
{
    IpAddr addr(ip, port);
    auto opened = this->open_socket(addr.is_ipv6());
    if (!opened)
        return opened;
    return this->connect(addr);
}

std::expected<void, sihd::util::Error> UdpSender::open_unix_and_connect(std::string_view path)
{
    auto opened = this->open_socket_unix();
    if (!opened)
        return opened;
    return this->connect_unix(path);
}

std::expected<void, sihd::util::Error> UdpSender::close()
{
    (void)_socket.shutdown();
    return _socket.close();
}

std::expected<size_t, sihd::util::Error> UdpSender::send(sihd::util::ArrCharView view)
{
    return _socket.send(view);
}

std::expected<void, sihd::util::Error> UdpSender::send_all(sihd::util::ArrCharView view)
{
    return _socket.send_all(view);
}

std::expected<void, sihd::util::Error> UdpSender::connect_unix(std::string_view path)
{
    return _socket.connect_unix(path);
}

std::expected<size_t, sihd::util::Error> UdpSender::send_to(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return _socket.send_to(addr, view);
}

std::expected<void, sihd::util::Error> UdpSender::send_to_all(const IpAddr & addr, sihd::util::ArrCharView view)
{
    return _socket.send_all_to(addr, view);
}

} // namespace sihd::net