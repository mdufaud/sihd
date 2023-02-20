#include <sihd/net/TcpClient.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::net
{

SIHD_UTIL_REGISTER_FACTORY(TcpClient)

SIHD_LOGGER;

TcpClient::TcpClient(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent)
{
    _connected = false;
    _poll.set_timeout(1);
    _poll.set_limit(1);
    _poll.add_observer(this);
    this->add_conf("poll_timeout", &TcpClient::set_poll_timeout);
}

TcpClient::~TcpClient() {}

bool TcpClient::set_poll_timeout(int milliseconds)
{
    _poll.set_timeout(milliseconds);
    return true;
}

bool TcpClient::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_STREAM, IPPROTO_TCP);
}

bool TcpClient::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    return _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

bool TcpClient::connect(const IpAddr & addr)
{
    _connected = _socket.connect(addr);
    return _connected;
}

bool TcpClient::connect(std::string_view path)
{
    _connected = _socket.connect_unix(path);
    return _connected;
}

bool TcpClient::open_and_connect(const IpAddr & ip)
{
    return this->open_socket(ip.prefers_ipv6()) && this->connect(ip);
}

bool TcpClient::open_unix_and_connect(std::string_view path)
{
    return this->open_socket_unix() && this->connect(path);
}

bool TcpClient::close()
{
    this->stop();
    _poll.clear_fds();
    _socket.shutdown();
    _connected = false;
    return _socket.close();
}

bool TcpClient::stop()
{
    _poll.stop();
    _poll.wait_stop();
    return true;
}

void TcpClient::_setup_poll()
{
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool TcpClient::run()
{
    this->_setup_poll();
    std::lock_guard lock(_poll_mutex);
    return _poll.run();
}

bool TcpClient::poll(int milliseconds)
{
    this->_setup_poll();
    return _poll.poll(milliseconds) > 0;
}

bool TcpClient::poll()
{
    this->_setup_poll();
    return _poll.poll(_poll.timeout()) > 0;
}

ssize_t TcpClient::receive(IpAddr & addr, sihd::util::IArray & arr)
{
    ssize_t ret = _socket.receive_from(addr, arr);
    if (_connected)
        _connected = ret != 0;
    return ret;
}

ssize_t TcpClient::receive(sihd::util::IArray & arr)
{
    ssize_t ret = _socket.receive(arr);
    if (_connected)
        _connected = ret != 0;
    return ret;
}

ssize_t TcpClient::receive(void *buf, size_t len)
{
    ssize_t ret = _socket.receive(buf, len);
    if (_connected)
        _connected = ret != 0;
    return ret;
}

ssize_t TcpClient::send(sihd::util::ArrCharView view)
{
    return _socket.send(view);
}

bool TcpClient::send_all(sihd::util::ArrCharView view)
{
    return _socket.send_all(view);
}

void TcpClient::handle(sihd::util::Poll *poll)
{
    auto events = poll->events();
    if (events.size() > 0)
    {
        auto event = events[0];
        if (event.fd == _socket.socket())
        {
            if (event.readable || event.closed)
            {
                if (event.closed)
                    _connected = false;
                this->notify_observers(this);
            }
            else if (event.error)
            {
                this->close();
            }
        }
    }
}

} // namespace sihd::net