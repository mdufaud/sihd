#include <sihd/net/UdpReceiver.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Result.hpp>

using namespace sihd::util;

namespace sihd::net
{

SIHD_REGISTER_FACTORY(UdpReceiver)

SIHD_LOGGER;

UdpReceiver::UdpReceiver(const std::string & name, Node *parent): Named(name, parent)
{
    _poll.set_timeout(1);
    _poll.set_limit(1);
    _poll.add_observer(this);
    _poll.set_service_wait_stop(true);
    this->add_conf("poll_timeout", &UdpReceiver::set_poll_timeout);
    this->add_conf("reuseaddr", &UdpReceiver::set_reuseaddr);
    this->set_reuseaddr(true);
}

UdpReceiver::~UdpReceiver()
{
    if (this->is_running())
        this->stop();
}

bool UdpReceiver::set_reuseaddr(bool active)
{
    _reuseaddr = active;
    if (_socket.is_open())
        return !SIHD_UNEXPECTED_LOG(_socket.set_reuseaddr(active));
    return true;
}

bool UdpReceiver::set_poll_timeout(int milliseconds)
{
    return _poll.set_timeout(milliseconds);
}

Result<void> UdpReceiver::open_socket_unix()
{
    if (_socket.is_open())
        return std::unexpected(Error(ErrorCode::already_exists, "socket already open"));
    return _socket.open(AF_UNIX, SOCK_DGRAM, 0);
}

Result<void> UdpReceiver::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return std::unexpected(Error(ErrorCode::already_exists, "socket already open"));
    auto res = _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (res && _reuseaddr)
        SIHD_UNEXPECTED_LOG(_socket.set_reuseaddr(true));
    return res;
}

bool UdpReceiver::socket_opened()
{
    return _socket.is_open();
}

Result<void> UdpReceiver::bind(const IpAddr & addr)
{
    return _socket.bind(addr);
}

Result<void> UdpReceiver::bind_unix(std::string_view path)
{
    return _socket.bind_unix(path);
}

Result<void> UdpReceiver::open_and_bind(const IpAddr & ip)
{
    auto opened = this->open_socket(ip.is_ipv6());
    if (!opened)
        return opened;
    return this->bind(ip);
}

Result<void> UdpReceiver::open_and_bind(std::string_view ip, int port)
{
    IpAddr addr(ip, port);
    auto opened = this->open_socket(addr.is_ipv6());
    if (!opened)
        return opened;
    return this->bind(addr);
}

Result<void> UdpReceiver::open_unix_and_bind(std::string_view path)
{
    auto opened = this->open_socket_unix();
    if (!opened)
        return opened;
    return this->bind_unix(path);
}

Result<void> UdpReceiver::close()
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

const Socket & UdpReceiver::socket() const
{
    return _socket;
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

Result<size_t> UdpReceiver::receive(void *buf, size_t len)
{
    return _socket.receive(buf, len);
}

Result<size_t> UdpReceiver::receive(IArray & arr)
{
    return _socket.receive(arr);
}

Result<size_t> UdpReceiver::receive(IpAddr & addr, void *buf, size_t len)
{
    return _socket.receive_from(addr, buf, len);
}

Result<size_t> UdpReceiver::receive(IpAddr & addr, IArray & arr)
{
    return _socket.receive_from(addr, arr);
}

} // namespace sihd::net