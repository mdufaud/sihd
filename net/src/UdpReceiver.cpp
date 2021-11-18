#include <sihd/net/UdpReceiver.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

LOGGER;

UdpReceiver::UdpReceiver(bool ipv6)
{
    this->_init();
    this->open_socket(ipv6);
}

UdpReceiver::UdpReceiver(const IpAddr & addr)
{
    this->_init();
    if (this->open_socket(addr.prefers_ipv6()))
        this->bind(addr);
}

UdpReceiver::UdpReceiver(const std::string & ip, int port)
{
    this->_init();
    IpAddr addr(ip, port, true);
    if (this->open_socket(addr.prefers_ipv6()))
        this->bind(addr);
}

UdpReceiver::UdpReceiver(const std::string & path)
{
    this->_init();
    if (this->open_socket_unix())
        this->bind(path);
}

UdpReceiver::~UdpReceiver()
{
    this->stop();
}

void    UdpReceiver::_init()
{
    _poll.set_timeout(1);
    _poll.set_limit(1);
    _poll.add_observer(this);
    this->add_conf("poll_timeout", &UdpReceiver::set_poll_timeout);
}

bool    UdpReceiver::set_poll_timeout(int milliseconds)
{
    _poll.set_timeout(milliseconds);
    return true;
}

bool    UdpReceiver::bind(const IpAddr & addr)
{
    return _socket.bind(addr);
}

bool    UdpReceiver::bind(const std::string & path)
{
    return _socket.bind(path);
}

bool    UdpReceiver::close()
{
    _poll.clear_fds();
    _socket.shutdown();
    return _socket.close();
}

bool    UdpReceiver::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_DGRAM, IPPROTO_UDP);
}

bool    UdpReceiver::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    bool ret = _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ret)
        _socket.set_reuseaddr(true);
    return ret;
}

bool    UdpReceiver::stop()
{
    _poll.stop();
    _poll.wait_stop();
    return true;
}

void    UdpReceiver::_setup_poll()
{
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool    UdpReceiver::run()
{
    this->_setup_poll();
    std::lock_guard lock(_poll_mutex);
    return _poll.run();
}

bool    UdpReceiver::poll(int milliseconds)
{
    this->_setup_poll();
    return _poll.poll(milliseconds) > 0;
}

bool    UdpReceiver::poll()
{
    this->_setup_poll();
    return _poll.poll(_poll.timeout()) > 0;
}

void    UdpReceiver::handle(sihd::util::Poll *poll)
{
    auto events = poll->get_events();
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


}