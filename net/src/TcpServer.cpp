#include <sihd/net/TcpServer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Task.hpp>

#if !defined(SIHD_DEFAULT_TCP_CONNECTIONS)
# define SIHD_DEFAULT_TCP_CONNECTIONS 50
#endif

#if !defined(SIHD_DEFAULT_TCP_POLL_TIMEOUT)
# define SIHD_DEFAULT_TCP_POLL_TIMEOUT 10
#endif

namespace sihd::net
{

using namespace std::placeholders;
using namespace sihd::util;

LOGGER;

TcpServer::TcpServer(bool ipv6)
{
    this->_init();
    this->open_socket(ipv6);
}

TcpServer::TcpServer(const IpAddr & addr)
{
    this->_init();
    if (this->open_socket(addr.prefers_ipv6()))
        this->bind(addr);
}

TcpServer::TcpServer(const std::string & ip, int port)
{
    this->_init();
    IpAddr addr(ip, port, true);
    if (this->open_socket(addr.prefers_ipv6()))
        this->bind(addr);
}

TcpServer::TcpServer(const std::string & path)
{
    this->_init();
    if (this->open_socket_unix())
        this->bind(path);
}

TcpServer::~TcpServer()
{
    this->stop_serving();
    if (_server_handler_ptr != nullptr)
        delete _server_handler_ptr;
}

bool    TcpServer::set_max_connections(size_t maxco)
{
    _max_connections = maxco;
    return true;
}

bool    TcpServer::set_poll_timeout(int milliseconds)
{
    _poll.set_timeout(milliseconds);
    return true;
}

void    TcpServer::_init()
{
    _server_handler_ptr = nullptr;
    _max_connections = SIHD_DEFAULT_TCP_CONNECTIONS;
    _poll.set_timeout(SIHD_DEFAULT_TCP_POLL_TIMEOUT);
    this->add_conf("max_connections", &TcpServer::set_max_connections);
    this->add_conf("poll_timeout", &TcpServer::set_poll_timeout);
}

bool    TcpServer::bind(const IpAddr & addr)
{
    return _socket.bind(addr);
}

bool    TcpServer::close()
{
    this->stop_serving();
    return _socket.close();
}

bool    TcpServer::open_socket_unix()
{
    if (_socket.is_open())
        return false;
    return _socket.open(AF_UNIX, SOCK_STREAM, IPPROTO_TCP);
}

bool    TcpServer::open_socket(bool ipv6)
{
    if (_socket.is_open())
        return false;
    bool ret = _socket.open(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ret)
        _socket.set_reuseaddr(true);
    return ret;
}

void    TcpServer::set_server_handler(INetServerHandler *handler)
{
    if (_server_handler_ptr != nullptr)
        delete _server_handler_ptr;
    _server_handler_ptr = handler;
}

void    TcpServer::_setup_poll(size_t maxco)
{
    if (_poll.get_read_handler() == nullptr)
    {
        _poll.set_read_handler(new Handler<int>(std::bind(&TcpServer::_handle_read, this, _1)))
            .set_write_handler(new Handler<int>(std::bind(&TcpServer::_handle_write, this, _1)))
            .set_prepoll_runnable(new Task(std::bind(&TcpServer::_handle_prepoll, this)))
            .set_postpoll_handler(new Handler<time_t, bool>(std::bind(&TcpServer::_handle_postpoll, this, _1, _2)));
    }
    _max_connections = maxco;
    _poll.set_max_fds(maxco + 1);
    _poll.clear_fds();
    _poll.set_read_fd(_socket.socket());
}

bool    TcpServer::serve(size_t nbco)
{
    bool ret = this->is_running();
    if (ret == false)
    {
        this->_setup_poll(nbco);
        ret = _server_handler_ptr != nullptr;
        if (!ret)
            LOG(error, "TcpServer: cannot serve without a server handler");
        ret = ret && _socket.listen(nbco);
        ret = ret && _poll.run();
    }
    return ret;
}

bool    TcpServer::stop_serving()
{
    bool running = this->is_running();
    if (running)
    {
        running = _poll.stop() == false;
        _socket.shutdown();
    }
    _waitable.notify_all();
    return running == false;
}

int     TcpServer::accept_client(IpAddr *client_ip)
{
    if (client_ip != nullptr)
        return _socket.accept(*client_ip);
    return _socket.accept();
}

bool    TcpServer::add_client_read(int socket)
{
    return _poll.set_read_fd(socket);
}

bool    TcpServer::add_client_write(int socket)
{
    return _poll.set_write_fd(socket);
}

bool    TcpServer::remove_client_read(int socket)
{
    return _poll.clear_fd(socket);
}

bool    TcpServer::remove_client_write(int socket)
{
    return _poll.clear_fd(socket);
}

bool    TcpServer::run()
{
    return this->serve(_max_connections);
}

bool    TcpServer::stop()
{
    return this->stop_serving();
}

bool    TcpServer::_handle_prepoll()
{
    return _server_handler_ptr->handle_before_activity(this);
}

void    TcpServer::_handle_postpoll(time_t nano, bool timed_out)
{
    if (timed_out == false)
    {
        _server_handler_ptr->handle_activity(this, nano);
        _waitable.notify(1);
    }
    else
        _server_handler_ptr->handle_no_activity(this, nano);
}

void    TcpServer::_handle_read(int socket)
{
    if (socket == _socket.socket())
        _server_handler_ptr->handle_new_client(this);
    else
        _server_handler_ptr->handle_client_read(this, socket);
}

void    TcpServer::_handle_write(int socket)
{
    _server_handler_ptr->handle_client_write(this, socket);
}

}