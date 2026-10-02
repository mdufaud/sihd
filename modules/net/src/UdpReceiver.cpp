#include <sihd/net/UdpReceiver.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(UdpReceiver)

SIHD_LOGGER;

UdpReceiver::UdpReceiver(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent)
{
    _poll.set_timeout(1);
    _poll.set_limit(1);
    _poll.add_observer(this);
    _poll.set_service_wait_stop(true);
    this->add_conf("poll_timeout", &UdpReceiver::set_poll_timeout);
}

UdpReceiver::~UdpReceiver()
{
    if (this->is_running())
        this->stop();
}

bool UdpReceiver::set_poll_timeout(int milliseconds)
{
    return _poll.set_timeout(milliseconds);
}

std::expected<void, sihd::util::Error> UdpReceiver::open_socket_unix()
{
    if (_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::already_exists, "UdpReceiver: socket already open"));
    return _socket.open(AF_UNIX, SOCK_DGRAM, 0);
}

std::expected<void, sihd::util::Error> UdpReceiver::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::already_exists, "UdpReceiver: socket already open"));
    auto res = _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (res)
        SIHD_UNEXPECTED_LOG(_socket.set_reuseaddr(true));
    return res;
}

std::expected<void, sihd::util::Error> UdpReceiver::bind(const IpAddr & addr)
{
    return _socket.bind(addr);
}

std::expected<void, sihd::util::Error> UdpReceiver::bind_unix(std::string_view path)
{
    return _socket.bind_unix(path);
}

std::expected<void, sihd::util::Error> UdpReceiver::open_and_bind(const IpAddr & ip)
{
    auto opened = this->open_socket(ip.is_ipv6());
    if (!opened)
        return opened;
    return this->bind(ip);
}

std::expected<void, sihd::util::Error> UdpReceiver::open_and_bind(std::string_view ip, int port)
{
    IpAddr addr(ip, port);
    auto opened = this->open_socket(addr.is_ipv6());
    if (!opened)
        return opened;
    return this->bind(addr);
}

std::expected<void, sihd::util::Error> UdpReceiver::open_unix_and_bind(std::string_view path)
{
    auto opened = this->open_socket_unix();
    if (!opened)
        return opened;
    return this->bind_unix(path);
}

std::expected<void, sihd::util::Error> UdpReceiver::close()
{
    (void)_socket.shutdown();
    return _socket.close();
}

bool UdpReceiver::on_stop()
{
    _poll.stop();
    _poll.clear_fds();
    return true;
}

void UdpReceiver::_setup_poll()
{
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool UdpReceiver::on_start()
{
    this->_setup_poll();
    this->service_set_ready();
    std::lock_guard lock(_poll_mutex);
    return _poll.start();
}

bool UdpReceiver::poll(int milliseconds)
{
    this->_setup_poll();
    return _poll.poll(milliseconds) > 0;
}

bool UdpReceiver::poll()
{
    this->_setup_poll();
    return _poll.poll(_poll.timeout()) > 0;
}

void UdpReceiver::handle(sihd::sys::Poll *poll)
{
    auto events = poll->events();
    if (events.size() > 0)
    {
        auto event = events[0];
        if (event.fd == _socket.socket())
        {
            if (event.readable || event.closed)
            {
                this->notify_observers(this);
            }
            else if (event.error)
            {
                poll->clear_fd(event.fd);
                (void)this->close();
            }
        }
    }
}

std::expected<size_t, sihd::util::Error> UdpReceiver::receive(void *buf, size_t len)
{
    return _socket.receive(buf, len);
}

std::expected<size_t, sihd::util::Error> UdpReceiver::receive(sihd::util::IArray & arr)
{
    return _socket.receive(arr);
}

std::expected<size_t, sihd::util::Error> UdpReceiver::receive(IpAddr & addr, void *buf, size_t len)
{
    return _socket.receive_from(addr, buf, len);
}

std::expected<size_t, sihd::util::Error> UdpReceiver::receive(IpAddr & addr, sihd::util::IArray & arr)
{
    return _socket.receive_from(addr, arr);
}

} // namespace sihd::net