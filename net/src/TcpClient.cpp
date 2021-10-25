#include <sihd/net/TcpClient.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

LOGGER;

TcpClient::TcpClient(bool ipv6)
{
    this->open_socket(ipv6);
}

TcpClient::TcpClient(const IpAddr & addr)
{
    if (this->open_socket(addr.prefer_ipv6()))
        this->connect(addr);
}

TcpClient::TcpClient(const std::string & ip, int port)
{
    IpAddr addr(ip, port, true);
    if (this->open_socket(addr.prefer_ipv6()))
        this->connect(addr);
}

TcpClient::TcpClient(const std::string & path)
{
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
    _array_ptr = nullptr;
    _poll_timeout_milliseconds = -1;
    _handler_ptr = nullptr;
    _poll.set_max_fds(1);
}

void    TcpClient::_delete_buffer()
{
    if (_array_ptr != nullptr && _array_owned == true)
    {
        delete _array_ptr;
        _array_ptr = nullptr;
        _array_owned = false;
    }
}

void    TcpClient::set_buffer(sihd::util::IArray *buffer)
{
    std::lock_guard lock(_poll_mutex);
    this->_delete_buffer();
    _array_owned = false;
    _array_ptr = buffer;
}

bool    TcpClient::set_buffer_size(size_t size)
{
    std::lock_guard lock(_poll_mutex);
    if (_array_ptr == nullptr)
    {
        _array_owned = true;
        _array_ptr = new sihd::util::ArrByte(size);
        return _array_ptr != nullptr;
    }
    return _array_ptr->reserve(size);
}

ssize_t     TcpClient::receive()
{
    if (_array_ptr == nullptr)
        throw std::runtime_error("TcpClient: no buffer set");
    return _socket.receive(*_array_ptr);
}

void    TcpClient::stop()
{
    _poll.stop();
    _waitable.notify_all();
}

void    TcpClient::_setup_poll()
{
    if (_array_ptr == nullptr)
        throw std::runtime_error("TcpClient: cannot poll with no buffer set");
    if (_poll.get_read_handler() == nullptr)
        _poll.set_read_handler(new sihd::util::Handler(this));
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

void    TcpClient::handle(int socket)
{
    if (socket == _socket.socket())
    {
        if (this->receive(*_array_ptr) <= 0)
            this->stop();
        else if (_handler_ptr != nullptr)
            _handler_ptr->handle(_array_ptr->cbuf(), _array_ptr->byte_size());
        _waitable.notify(1);
    }
}

void    TcpClient::set_handler(sihd::util::IHandler<const void *, size_t> *handler)
{
    std::lock_guard lock(_poll_mutex);
    if (_handler_ptr != nullptr)
        delete _handler_ptr;
    _handler_ptr = handler;
}

}