#include <chrono>

#include <sihd/net/DeviceTcpServer.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(DeviceTcpServer)

SIHD_LOGGER;

DeviceTcpServer::DeviceTcpServer(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _tcp_server("tcp-server"),
    _port(0),
    _buffer_capacity(4096),
    _channel_rx(nullptr),
    _channel_tx(nullptr),
    _channel_client_count(nullptr)
{
    _tcp_server.set_server_handler(&_server_handler);
    _worker.set_runnable(this);
    this->add_conf("host", &DeviceTcpServer::set_host);
    this->add_conf("port", &DeviceTcpServer::set_port);
    this->add_conf("unix_path", &DeviceTcpServer::set_unix_path);
    this->add_conf("buffer_capacity", &DeviceTcpServer::set_buffer_capacity);
    this->add_conf("poll_timeout", &DeviceTcpServer::set_poll_timeout);
    this->add_conf("poll_limit", &DeviceTcpServer::set_poll_limit);
    this->add_conf("queue_size", &DeviceTcpServer::set_queue_size);
    this->add_conf("max_clients", &DeviceTcpServer::set_max_clients);
}

DeviceTcpServer::~DeviceTcpServer()
{
    if (this->is_running())
        this->stop();
}

bool DeviceTcpServer::is_running() const
{
    return _worker.is_worker_running();
}

bool DeviceTcpServer::set_host(std::string_view host)
{
    _host = host;
    return true;
}

bool DeviceTcpServer::set_port(int port)
{
    _port = port;
    return true;
}

bool DeviceTcpServer::set_unix_path(std::string_view path)
{
    _unix_path = path;
    return true;
}

bool DeviceTcpServer::set_buffer_capacity(size_t capacity)
{
    _buffer_capacity = capacity;
    return true;
}

bool DeviceTcpServer::set_poll_timeout(int milliseconds)
{
    return _tcp_server.set_poll_timeout(milliseconds);
}

bool DeviceTcpServer::set_poll_limit(int limit)
{
    return _tcp_server.set_poll_limit(limit);
}

bool DeviceTcpServer::set_queue_size(size_t size)
{
    return _tcp_server.set_queue_size(size);
}

bool DeviceTcpServer::set_max_clients(size_t max)
{
    return _server_handler.set_max_clients(max);
}

bool DeviceTcpServer::on_init()
{
    this->add_unlinked_channel_resizable("rx", sihd::util::Type::TYPE_BYTE, 1, _buffer_capacity);
    this->add_unlinked_channel_resizable("tx", sihd::util::Type::TYPE_BYTE, 1, _buffer_capacity);
    this->add_unlinked_channel("client_count", sihd::util::Type::TYPE_INT, 1);
    return true;
}

bool DeviceTcpServer::on_start()
{
    _channel_rx = this->get_channel("rx");
    _channel_tx = this->get_channel("tx");
    _channel_client_count = this->get_channel("client_count");

    if (_channel_rx == nullptr || _channel_tx == nullptr || _channel_client_count == nullptr)
        return false;

    if (_unix_path.empty() && (_host.empty() || _port <= 0))
    {
        SIHD_LOG(error, "DeviceTcpServer: host+port or unix_path must be set");
        return false;
    }

    bool bound;
    if (!_unix_path.empty())
        bound = _tcp_server.open_unix_and_bind(_unix_path);
    else
        bound = _tcp_server.open_and_bind(_host, _port);

    if (!bound)
    {
        SIHD_LOG(error, "DeviceTcpServer: failed to bind");
        return false;
    }

    this->observe_channel(_channel_tx);
    _server_handler.add_observer(this);
    _channel_client_count->write<int32_t>(0, 0);

    util::Defer cleanup([this] {
        _server_handler.remove_observer(this);
        _tcp_server.close();
    });

    if (!_worker.start_worker("DeviceTcpServer"))
    {
        return false;
    }

    if (!_tcp_server.wait_ready(std::chrono::seconds(1)))
    {
        _tcp_server.stop();
        _worker.stop_worker();
        return false;
    }

    cleanup.cancel();
    return true;
}

bool DeviceTcpServer::on_stop()
{
    _tcp_server.stop();
    _worker.stop_worker();
    _server_handler.remove_observer(this);
    _tcp_server.close();
    return true;
}

bool DeviceTcpServer::on_reset()
{
    _channel_rx = nullptr;
    _channel_tx = nullptr;
    _channel_client_count = nullptr;
    return true;
}

bool DeviceTcpServer::run()
{
    _tcp_server.start();
    return true;
}

void DeviceTcpServer::handle(sihd::core::Channel *c)
{
    if (c == _channel_tx)
    {
        auto clients = _server_handler.clients();
        for (const auto & client : clients)
        {
            _server_handler.send_to_client(client, *c->array());
        }
    }
}

void DeviceTcpServer::handle(BasicServerHandler *handler)
{
    for (const auto & client : handler->read_activity())
    {
        if (client->disconnected || client->error)
        {
            handler->remove_client(client);
        }
        else
        {
            _channel_rx->write(client->read_array);
        }
    }

    if (!handler->new_clients().empty() || !handler->read_activity().empty())
    {
        _channel_client_count->write<int32_t>(0, static_cast<int32_t>(handler->client_count()));
    }
}

} // namespace sihd::net
