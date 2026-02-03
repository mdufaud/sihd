#include <sihd/net/UdpReceiver.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::net
{

SIHD_UTIL_REGISTER_FACTORY(UdpReceiver)

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

bool UdpReceiver::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_DGRAM, IPPROTO_UDP);
}

bool UdpReceiver::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    bool ret = _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ret)
        _socket.set_reuseaddr(true);
    return ret;
}

bool UdpReceiver::bind(const IpAddr & addr)
{
    return _socket.bind(addr);
}

bool UdpReceiver::bind_unix(std::string_view path)
{
    return _socket.bind_unix(path);
}

bool UdpReceiver::open_and_bind(const IpAddr & ip)
{
    return this->open_socket(ip.is_ipv6()) && this->bind(ip);
}

bool UdpReceiver::open_and_bind(std::string_view ip, int port)
{
    IpAddr addr(ip, port);
    return this->open_socket(addr.is_ipv6()) && this->bind(addr);
}

bool UdpReceiver::open_unix_and_bind(std::string_view path)
{
    return this->open_socket_unix() && this->bind(path);
}

bool UdpReceiver::close()
{
    _socket.shutdown();
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

void UdpReceiver::handle(sihd::util::Poll *poll)
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
                this->close();
            }
        }
    }
}

ssize_t UdpReceiver::receive(void *buf, size_t len)
{
    return _socket.receive(buf, len);
}

ssize_t UdpReceiver::receive(sihd::util::IArray & arr)
{
    return _socket.receive(arr);
}

ssize_t UdpReceiver::receive(IpAddr & addr, void *buf, size_t len)
{
    return _socket.receive_from(addr, buf, len);
}

ssize_t UdpReceiver::receive(IpAddr & addr, sihd::util::IArray & arr)
{
    return _socket.receive_from(addr, arr);
}

} // namespace sihd::net