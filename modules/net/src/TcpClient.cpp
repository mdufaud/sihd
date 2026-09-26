#include <sihd/net/Socket.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(TcpClient)

SIHD_LOGGER;

TcpClient::TcpClient(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent)
{
    _connected = false;
    _poll.set_timeout(1);
    _poll.set_limit(1);
    _poll.add_observer(this);
    _poll.set_service_wait_stop(true);
    this->add_conf("poll_timeout", &TcpClient::set_poll_timeout);
}

TcpClient::~TcpClient()
{
    if (this->is_running())
        this->stop();
}

void TcpClient::set_tls_context(sihd::crypto::TlsContext ctx)
{
    _socket.set_tls_context(std::move(ctx));
}

bool TcpClient::set_poll_timeout(int milliseconds)
{
    return _poll.set_timeout(milliseconds);
}

bool TcpClient::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_STREAM, 0);
}

bool TcpClient::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    return _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

bool TcpClient::connect(const IpAddr & addr, int timeout_ms)
{
    _connected = _socket.connect(addr, timeout_ms);
    return _connected;
}

bool TcpClient::connect(std::string_view path)
{
    _connected = _socket.connect_unix(path);
    return _connected;
}

bool TcpClient::reconnect(int timeout_ms)
{
    _connected = _socket.reconnect(timeout_ms);
    return _connected;
}

bool TcpClient::open_and_connect(const IpAddr & ip, int timeout_ms)
{
    return this->open_socket(ip.is_ipv6()) && this->connect(ip, timeout_ms);
}

bool TcpClient::open_and_connect(std::string_view ip, int port, int timeout_ms)
{
    IpAddr addr(ip, port);
    return this->open_socket(addr.is_ipv6()) && this->connect(addr, timeout_ms);
}

bool TcpClient::open_unix_and_connect(std::string_view path)
{
    return this->open_socket_unix() && this->connect(path);
}

bool TcpClient::close()
{
    _socket.shutdown();
    _connected = false;
    return _socket.close();
}

bool TcpClient::on_stop()
{
    _poll.stop();
    _poll.clear_fds();
    return true;
}

void TcpClient::_setup_poll()
{
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool TcpClient::on_start()
{
    this->_setup_poll();
    this->service_set_ready();
    std::lock_guard lock(_poll_mutex);
    return _poll.start();
}

bool TcpClient::poll(int milliseconds)
{
    if (!_connected)
        return false;
    this->_setup_poll();
    return _poll.poll(milliseconds) > 0;
}

bool TcpClient::poll()
{
    if (!_connected)
        return false;
    this->_setup_poll();
    return _poll.poll(_poll.timeout()) > 0;
}

ssize_t TcpClient::receive(IpAddr & addr, sihd::util::IArray & arr)
{
    ssize_t ret = _socket.receive_from(addr, arr);
    if (_connected)
        _connected = ret > 0 || _socket.retryable();
    return ret;
}

ssize_t TcpClient::receive(sihd::util::IArray & arr)
{
    ssize_t ret = _socket.receive(arr);
    if (_connected)
        _connected = ret > 0 || _socket.retryable();
    return ret;
}

ssize_t TcpClient::receive(void *buf, size_t len)
{
    ssize_t ret = _socket.receive(buf, len);
    if (_connected)
        _connected = ret > 0 || _socket.retryable();
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

void TcpClient::handle(sihd::sys::Poll *poll)
{
    const auto & events = poll->events();
    if (events.empty() || events[0].fd != _socket.socket())
        return;

    const auto event = events[0];
    if (event.error)
    {
        const std::optional<int> so_error = _socket.get_error();
        if (so_error && *so_error != 0)
            SIHD_LOG(error, "TcpClient: socket error: {}", sihd::sys::os::error_str(*so_error));
        else if (!so_error)
            SIHD_LOG(error, "TcpClient: socket error: {}", sihd::sys::os::last_error_str());
        poll->clear_fd(event.fd);
        this->close();
        return;
    }
    if (event.closed)
    {
        // the FIN notification is the last one: observers drain the tail, then disconnect
        if (_connected)
            this->notify_observers(this);
        _connected = false;
        SIHD_LOG(debug, "TcpClient: connection closed by peer");
        poll->clear_fd(event.fd);
    }
    else if (event.readable)
    {
        this->notify_observers(this);
    }
}

} // namespace sihd::net