#include <sihd/net/TcpServer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::net
{

using namespace std::placeholders;
using namespace sihd::util;

SIHD_UTIL_REGISTER_FACTORY(TcpServer)

SIHD_LOGGER;

TcpServer::TcpServer(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent)
{
    _server_handler_ptr = nullptr;
    _poll.set_service_wait_stop(true);
    _poll.add_observer(this);

    this->set_queue_size(50);
    this->set_poll_timeout(10);
    this->set_poll_limit(10);

    this->add_conf("queue_size", &TcpServer::set_queue_size);
    this->add_conf("poll_timeout", &TcpServer::set_poll_timeout);
    this->add_conf("poll_limit", &TcpServer::set_poll_limit);
}

TcpServer::~TcpServer()
{
    if (this->is_running())
        this->stop();
}

bool TcpServer::set_queue_size(size_t size)
{
    _queue_size = size;
    return true;
}

bool TcpServer::set_poll_limit(int limit)
{
    return _poll.set_limit(limit);
}

bool TcpServer::set_poll_timeout(int milliseconds)
{
    _poll.set_timeout(milliseconds);
    return true;
}

bool TcpServer::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_STREAM, IPPROTO_TCP);
}

bool TcpServer::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    bool ret = _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ret)
        _socket.set_reuseaddr(true);
    return ret;
}

bool TcpServer::bind(const IpAddr & addr)
{
    return _socket.bind(addr);
}

bool TcpServer::open_and_bind(const IpAddr & ip)
{
    return this->open_socket(ip.is_ipv6()) && this->bind(ip);
}

bool TcpServer::open_and_bind(std::string_view ip, int port)
{
    IpAddr addr(ip, port);
    return this->open_socket(addr.is_ipv6()) && this->bind(addr);
}

bool TcpServer::open_unix_and_bind(std::string_view path)
{
    return this->open_socket_unix() && this->bind(path);
}

bool TcpServer::close()
{
    _socket.shutdown();
    return _socket.close();
}

void TcpServer::set_server_handler(INetServerHandler *handler)
{
    if (_server_handler_ptr != nullptr)
        delete _server_handler_ptr;
    _server_handler_ptr = handler;
}

void TcpServer::_setup_poll()
{
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

int TcpServer::accept_client(IpAddr *client_ip)
{
    if (client_ip != nullptr)
        return _socket.accept(*client_ip);
    return _socket.accept();
}

bool TcpServer::add_client_read(int socket)
{
    return _poll.set_read_fd(socket);
}

bool TcpServer::add_client_write(int socket)
{
    return _poll.set_write_fd(socket);
}

bool TcpServer::remove_client_read(int socket)
{
    return _poll.rm_read_fd(socket);
}

bool TcpServer::remove_client_write(int socket)
{
    return _poll.rm_write_fd(socket);
}

bool TcpServer::on_start()
{
    bool ret = _poll.is_running();
    if (ret == false)
    {
        this->_setup_poll();
        ret = _server_handler_ptr != nullptr;
        if (!ret)
            SIHD_LOG(error, "TcpServer: cannot serve without a server handler");
        ret = ret && _socket.listen(this->queue_size());
        ret = ret && _poll.start();
    }
    return ret;
}

bool TcpServer::on_stop()
{
    _poll.stop();
    _poll.clear_fds();
    return true;
}

void TcpServer::handle(sihd::util::Poll *poll)
{
    if (poll->polling_timeout())
        _server_handler_ptr->handle_no_activity(this, poll->polling_time());
    else
        _server_handler_ptr->handle_activity(this, poll->polling_time());
    auto events = poll->events();
    for (const auto & event : events)
    {
        if (event.fd == _socket.socket())
        {
            if (event.readable)
                _server_handler_ptr->handle_new_client(this);
            else if (event.closed || event.error)
            {
                this->stop();
                break;
            }
        }
        else if (event.readable)
        {
            _server_handler_ptr->handle_client_read(this, event.fd);
        }
        else if (event.writable)
        {
            _server_handler_ptr->handle_client_write(this, event.fd);
        }
    }
    _server_handler_ptr->handle_after_activity(this);
}

} // namespace sihd::net