#include <chrono>
#include <set>

#include <sihd/net/ClientFrame.hpp>
#include <sihd/net/DevTcpServer.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

using namespace sihd::util;

namespace sihd::net
{

SIHD_REGISTER_FACTORY(DevTcpServer)

SIHD_LOGGER;

DevTcpServer::DevTcpServer(const std::string & name, Node *parent):
    sihd::core::Device(name, parent),
    _tcp_server("tcp-server"),
    _port(0),
    _buffer_capacity(4096),
    _channel_rx(nullptr),
    _channel_tx(nullptr),
    _channel_client_count(nullptr),
    _channel_rx_from(nullptr),
    _channel_tx_to(nullptr),
    _channel_connected(nullptr),
    _channel_disconnected(nullptr)
{
    _tcp_server.set_server_handler(&_server_handler);
    _worker.set_runnable(this);
    this->add_conf("host", &DevTcpServer::set_host);
    this->add_conf("port", &DevTcpServer::set_port);
    this->add_conf("unix_path", &DevTcpServer::set_unix_path);
    this->add_conf("buffer_capacity", &DevTcpServer::set_buffer_capacity);
    this->add_conf("poll_timeout", &DevTcpServer::set_poll_timeout);
    this->add_conf("poll_limit", &DevTcpServer::set_poll_limit);
    this->add_conf("queue_size", &DevTcpServer::set_queue_size);
    this->add_conf("max_clients", &DevTcpServer::set_max_clients);
}

DevTcpServer::~DevTcpServer()
{
    if (this->is_running())
        this->stop();
}

bool DevTcpServer::is_running() const
{
    return _worker.is_worker_running();
}

bool DevTcpServer::set_host(std::string_view host)
{
    _host = host;
    return true;
}

bool DevTcpServer::set_port(int port)
{
    _port = port;
    return true;
}

bool DevTcpServer::set_unix_path(std::string_view path)
{
    _unix_path = path;
    return true;
}

bool DevTcpServer::set_buffer_capacity(size_t capacity)
{
    _buffer_capacity = capacity;
    return true;
}

bool DevTcpServer::set_poll_timeout(int milliseconds)
{
    return _tcp_server.set_poll_timeout(milliseconds);
}

bool DevTcpServer::set_poll_limit(int limit)
{
    return _tcp_server.set_poll_limit(limit);
}

bool DevTcpServer::set_queue_size(size_t size)
{
    return _tcp_server.set_queue_size(size);
}

bool DevTcpServer::set_max_clients(size_t max)
{
    return _server_handler.set_max_clients(max);
}

bool DevTcpServer::on_init()
{
    this->add_unlinked_channel_resizable("rx", Type::TYPE_BYTE, 1, _buffer_capacity);
    this->add_unlinked_channel_resizable("tx", Type::TYPE_BYTE, 1, _buffer_capacity);
    this->add_unlinked_channel("client_count", Type::TYPE_INT, 1);
    const size_t frame_capacity = ClientFrame::header_size + _buffer_capacity;
    sihd::core::Channel *rx_from = this->add_unlinked_channel_resizable("rx_from", Type::TYPE_BYTE, 1, frame_capacity);
    sihd::core::Channel *tx_to = this->add_unlinked_channel_resizable("tx_to", Type::TYPE_BYTE, 1, frame_capacity);
    // the same payload sent twice by a client, or to a client, is two messages
    if (rx_from != nullptr)
        rx_from->set_write_on_change(false);
    if (tx_to != nullptr)
        tx_to->set_write_on_change(false);
    this->add_unlinked_channel("connected", Type::TYPE_ULONG, 1);
    this->add_unlinked_channel("disconnected", Type::TYPE_ULONG, 1);
    return true;
}

bool DevTcpServer::on_start()
{
    auto rx = this->get_channel("rx");
    auto tx = this->get_channel("tx");
    auto client_count = this->get_channel("client_count");
    auto rx_from = this->get_channel("rx_from");
    auto tx_to = this->get_channel("tx_to");
    auto connected = this->get_channel("connected");
    auto disconnected = this->get_channel("disconnected");
    if (SIHD_UNEXPECTED_LOG(rx) || SIHD_UNEXPECTED_LOG(tx) || SIHD_UNEXPECTED_LOG(client_count)
        || SIHD_UNEXPECTED_LOG(rx_from) || SIHD_UNEXPECTED_LOG(tx_to) || SIHD_UNEXPECTED_LOG(connected)
        || SIHD_UNEXPECTED_LOG(disconnected))
        return false;

    _channel_rx = *rx;
    _channel_tx = *tx;
    _channel_client_count = *client_count;
    _channel_rx_from = *rx_from;
    _channel_tx_to = *tx_to;
    _channel_connected = *connected;
    _channel_disconnected = *disconnected;

    if (_unix_path.empty() && (_host.empty() || _port <= 0))
    {
        SIHD_LOG(error, "DevTcpServer: host+port or unix_path must be set");
        return false;
    }

    auto bound = !_unix_path.empty() ? _tcp_server.open_unix_and_bind(_unix_path) //
                                     : _tcp_server.open_and_bind(_host, _port);
    if (SIHD_UNEXPECTED_LOG(bound))
        return false;

    this->observe_channel(_channel_tx);
    this->observe_channel(_channel_tx_to);
    _server_handler.add_observer(this);
    _channel_client_count->write<int32_t>(0, 0);

    util::Defer cleanup([this] {
        _server_handler.remove_observer(this);
        (void)_tcp_server.close();
    });

    if (!_worker.start_worker("DevTcpServer"))
    {
        return false;
    }

    if (!_tcp_server.wait_ready(std::chrono::seconds(1)))
    {
        SIHD_LOG(error, "DevTcpServer: the server did not become ready in time");
        _tcp_server.stop();
        _worker.stop_worker();
        return false;
    }

    cleanup.cancel();
    return true;
}

bool DevTcpServer::on_stop()
{
    _tcp_server.stop();
    _worker.stop_worker();
    _server_handler.remove_observer(this);
    (void)_tcp_server.close();
    return true;
}

bool DevTcpServer::on_reset()
{
    _channel_rx = nullptr;
    _channel_tx = nullptr;
    _channel_client_count = nullptr;
    _channel_rx_from = nullptr;
    _channel_tx_to = nullptr;
    _channel_connected = nullptr;
    _channel_disconnected = nullptr;
    return true;
}

bool DevTcpServer::run()
{
    if (_tcp_server.start() == false)
    {
        SIHD_LOG(error, "DevTcpServer: could not start the server");
        return false;
    }
    return true;
}

void DevTcpServer::handle(sihd::core::Channel *c)
{
    if (c == _channel_tx)
    {
        auto clients = _server_handler.clients();
        for (const auto & client : clients)
        {
            if (_server_handler.send_to_client(client, *c->array()) == false)
                SIHD_LOG(warning, "DevTcpServer: could not send to a client");
        }
    }
    else if (c == _channel_tx_to)
    {
        this->_send_to_one(c);
    }
}

void DevTcpServer::_send_to_one(const sihd::core::Channel *c)
{
    auto frame = ClientFrame::decode(ArrByteView(*c->array()));
    if (SIHD_UNEXPECTED_LOG(frame))
        return;
    // a write for a client that left is dropped: a new client never gets an id twice
    auto client = _server_handler.client_by_id(frame->client_id());
    if (client == nullptr)
    {
        SIHD_LOG(warning, "DevTcpServer: client {} is gone, its write is dropped", frame->client_id());
        return;
    }
    if (_server_handler.send_to_client(client, frame->payload()) == false)
        SIHD_LOG(warning, "DevTcpServer: could not send to client {}", frame->client_id());
}

void DevTcpServer::handle(BasicServerHandler *handler)
{
    for (const auto & client : handler->new_clients())
        _channel_connected->write<uint64_t>(0, client->id);

    // a client failing on a read and on a write of the same round leaves once
    std::set<uint64_t> gone;
    for (const auto & client : handler->read_activity())
    {
        if (client->disconnected || client->error)
        {
            handler->remove_client(client);
            gone.insert(client->id);
        }
        else
        {
            _channel_rx->write(client->read_array);
            _channel_rx_from->write(ClientFrame(client->id, client->read_array).encode());
        }
    }
    for (const auto & client : handler->write_activity())
    {
        if (client->error)
            gone.insert(client->id);
    }
    for (const uint64_t id : gone)
        _channel_disconnected->write<uint64_t>(0, id);

    if (!handler->new_clients().empty() || !handler->read_activity().empty() || !gone.empty())
    {
        _channel_client_count->write<int32_t>(0, static_cast<int32_t>(handler->client_count()));
    }
}

} // namespace sihd::net
