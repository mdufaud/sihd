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
    this->add_conf("recv_timeout", &TcpClient::set_recv_timeout);
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

bool TcpClient::set_recv_timeout(int milliseconds)
{
    if (milliseconds < 0)
    {
        SIHD_LOG(error, "TcpClient: recv timeout cannot be negative: {}", milliseconds);
        return false;
    }
    _recv_timeout = milliseconds;
    if (_socket.is_open())
        return !SIHD_UNEXPECTED_LOG(_socket.set_recv_timeout(_recv_timeout));
    return true;
}

std::expected<void, sihd::util::Error> TcpClient::open_socket_unix()
{
    if (_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::already_exists, "TcpClient: socket already open"));
    return _socket.open(AF_UNIX, SOCK_STREAM, 0);
}

std::expected<void, sihd::util::Error> TcpClient::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return std::unexpected(
            sihd::util::Error(sihd::util::ErrorCode::already_exists, "TcpClient: socket already open"));
    return _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

std::expected<void, sihd::util::Error> TcpClient::connect(const IpAddr & addr, int timeout_ms)
{
    auto res = _socket.connect(addr, timeout_ms);
    _connected = res.has_value();
    if (_connected)
        SIHD_UNEXPECTED_LOG(_socket.set_recv_timeout(_recv_timeout));
    return res;
}

std::expected<void, sihd::util::Error> TcpClient::connect(std::string_view path)
{
    auto res = _socket.connect_unix(path);
    _connected = res.has_value();
    if (_connected)
        SIHD_UNEXPECTED_LOG(_socket.set_recv_timeout(_recv_timeout));
    return res;
}

std::expected<void, sihd::util::Error> TcpClient::reconnect(int timeout_ms)
{
    auto res = _socket.reconnect(timeout_ms);
    _connected = res.has_value();
    if (_connected)
        SIHD_UNEXPECTED_LOG(_socket.set_recv_timeout(_recv_timeout));
    return res;
}

std::expected<void, sihd::util::Error> TcpClient::open_and_connect(const IpAddr & ip, int timeout_ms)
{
    auto opened = this->open_socket(ip.is_ipv6());
    if (!opened)
        return opened;
    return this->connect(ip, timeout_ms);
}

std::expected<void, sihd::util::Error> TcpClient::open_and_connect(std::string_view ip, int port, int timeout_ms)
{
    IpAddr addr(ip, port);
    auto opened = this->open_socket(addr.is_ipv6());
    if (!opened)
        return opened;
    return this->connect(addr, timeout_ms);
}

std::expected<void, sihd::util::Error> TcpClient::open_unix_and_connect(std::string_view path)
{
    auto opened = this->open_socket_unix();
    if (!opened)
        return opened;
    return this->connect(path);
}

std::expected<void, sihd::util::Error> TcpClient::close()
{
    (void)_socket.shutdown();
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

std::expected<size_t, sihd::util::Error> TcpClient::receive(IpAddr & addr, sihd::util::IArray & arr)
{
    auto res = _socket.receive_from(addr, arr);
    if (_connected)
        _connected = (res && res.value() > 0) || (!res && res.error().retryable());
    return res;
}

std::expected<size_t, sihd::util::Error> TcpClient::receive(sihd::util::IArray & arr)
{
    auto res = _socket.receive(arr);
    if (_connected)
        _connected = (res && res.value() > 0) || (!res && res.error().retryable());
    return res;
}

std::expected<size_t, sihd::util::Error> TcpClient::receive(void *buf, size_t len)
{
    auto res = _socket.receive(buf, len);
    if (_connected)
        _connected = (res && res.value() > 0) || (!res && res.error().retryable());
    return res;
}

std::expected<size_t, sihd::util::Error> TcpClient::send(sihd::util::ArrCharView view)
{
    return _socket.send(view);
}

std::expected<void, sihd::util::Error> TcpClient::send_all(sihd::util::ArrCharView view)
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
            SIHD_LOG(error, "TcpClient: socket error: {}", sihd::sys::os::last_socket_error_str());
        poll->clear_fd(event.fd);
        (void)this->close();
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