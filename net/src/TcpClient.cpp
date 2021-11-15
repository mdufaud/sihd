#include <sihd/net/TcpClient.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

LOGGER;

TcpClient::TcpClient()
{
    this->_init();
}

TcpClient::TcpClient(bool ipv6)
{
    this->_init();
    this->open_socket(ipv6);
}

TcpClient::TcpClient(const IpAddr & addr)
{
    this->_init();
    if (this->open_socket(addr.prefers_ipv6()))
        this->connect(addr);
}

TcpClient::TcpClient(const std::string & ip, int port)
{
    this->_init();
    IpAddr addr(ip, port, true);
    if (this->open_socket(addr.prefers_ipv6()))
        this->connect(addr);
}

TcpClient::TcpClient(const std::string & path)
{
    this->_init();
    if (this->open_socket_unix())
        this->connect(path);
}

TcpClient::~TcpClient()
{
}

bool    TcpClient::connect(const IpAddr & addr)
{
    return _socket.connect(addr);
}

bool    TcpClient::connect(const std::string & ip, int port)
{
    IpAddr addr(ip, port, true);
    return this->connect(addr);
}

bool    TcpClient::connect(const std::string & path)
{
    return _socket.connect_unix(path); 
}

bool    TcpClient::close()
{
    _poll.clear_fds();
    _socket.shutdown();
    return _socket.close();
}

bool    TcpClient::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_STREAM, IPPROTO_TCP);
}

bool    TcpClient::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    return _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

void    TcpClient::_init()
{
    _array_owned = false;
    _poll_timeout_milliseconds = -1;
    _poll.set_limit(1);
    _poll.add_observer(this);
    this->add_conf("poll_timeout", &TcpClient::set_poll_timeout);
}

bool    TcpClient::stop()
{
    _poll.stop();
    _poll.wait_stop();
    return true;
}

void    TcpClient::_setup_poll()
{
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool    TcpClient::run()
{
    this->_setup_poll();
    _poll.set_timeout(_poll_timeout_milliseconds);
    std::lock_guard lock(_poll_mutex);
    return _poll.run();
}

bool    TcpClient::poll(int milliseconds)
{
    this->_setup_poll();
    return _poll.poll(milliseconds) > 0;
}

bool    TcpClient::poll()
{
    this->_setup_poll();
    return _poll.poll(_poll_timeout_milliseconds) > 0;
}

void    TcpClient::handle(sihd::util::Poll *poll)
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


}