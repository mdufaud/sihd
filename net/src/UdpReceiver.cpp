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
    if (this->open_socket(addr.prefer_ipv6()))
        this->bind(addr);
}

UdpReceiver::UdpReceiver(const std::string & ip, int port)
{
    IpAddr addr(ip, port, true);
    if (this->open_socket(addr.prefer_ipv6()))
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
    if (_handler_ptr != nullptr)
        delete _handler_ptr;
}

void    UdpReceiver::_init()
{
    _array_owned = false;
    _array_ptr = nullptr;
    _poll_timeout_milliseconds = -1;
    _handler_ptr = nullptr;
    _poll.set_max_fds(1);
}

void    UdpReceiver::_delete_buffer()
{
    if (_array_ptr != nullptr && _array_owned == true)
    {
        delete _array_ptr;
        _array_ptr = nullptr;
        _array_owned = false;
    }
}

void    UdpReceiver::set_buffer(sihd::util::IArray *buffer)
{
    std::lock_guard lock(_poll_mutex);
    this->_delete_buffer();
    _array_owned = false;
    _array_ptr = buffer;
}

bool    UdpReceiver::set_buffer_size(size_t size)
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

bool    UdpReceiver::bind(const IpAddr & addr)
{
    return _socket.bind(addr);
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

ssize_t     UdpReceiver::receive()
{
    if (_array_ptr == nullptr)
        throw std::runtime_error("UdpReceiver: no buffer set");
    return _socket.receive(*_array_ptr);
}

ssize_t     UdpReceiver::receive_from(IpAddr & addr)
{
    if (_array_ptr == nullptr)
        throw std::runtime_error("UdpReceiver: no buffer set");
    return _socket.receive_from(addr, *_array_ptr);
}

void    UdpReceiver::stop()
{
    _poll.stop();
    _waitable.notify_all();
}

void    UdpReceiver::_setup_poll()
{
    if (_array_ptr == nullptr)
        throw std::runtime_error("UdpReceiver: cannot poll with no buffer set");
    if (_poll.get_read_handler() == nullptr)
        _poll.set_read_handler(new sihd::util::Handler(this));
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool    UdpReceiver::run()
{
    this->_setup_poll();
    _poll.set_timeout(_poll_timeout_milliseconds);
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
    return _poll.poll(_poll_timeout_milliseconds) > 0;
}

void    UdpReceiver::handle(int socket)
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

void    UdpReceiver::set_handler(sihd::util::IHandler<const void *, size_t> *handler)
{
    std::lock_guard lock(_poll_mutex);
    if (_handler_ptr != nullptr)
        delete _handler_ptr;
    _handler_ptr = handler;
}

}