#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/util/Logger.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <unistd.h>
#endif

namespace sihd::net
{

SIHD_LOGGER;

BasicServerHandler::BasicServerHandler()
{
    _last_time = 0;
    _poll_time = 0;
    _server = nullptr;
    this->set_max_clients(512);
    this->add_conf("max_clients", &BasicServerHandler::set_max_clients);
}

BasicServerHandler::~BasicServerHandler() = default;

void BasicServerHandler::_reset()
{
    _read_event_lst.clear();
    _write_event_lst.clear();
    _connect_event_lst.clear();
}

void BasicServerHandler::_add_time_to_clients()
{
    if (_last_time <= 0)
        return;
    std::lock_guard lock(_mutex);
    sihd::util::Duration t = _clock.now() - _last_time;
    for (auto & [fd, client] : _client_map)
    {
        client->time_total += t;
    }
}

bool BasicServerHandler::set_max_clients(size_t max)
{
    _max_clients = max;
    return true;
}

size_t BasicServerHandler::client_count() const
{
    std::lock_guard lock(_mutex);
    return _client_map.size();
}

BasicServerHandler::ClientPtr BasicServerHandler::client(int socket)
{
    std::lock_guard lock(_mutex);
    auto it = _client_map.find(socket);
    return it != _client_map.end() ? it->second : nullptr;
}

std::vector<BasicServerHandler::ClientPtr> BasicServerHandler::clients() const
{
    std::lock_guard lock(_mutex);
    std::vector<ClientPtr> result;
    result.reserve(_client_map.size());
    for (const auto & [fd, client] : _client_map)
    {
        result.push_back(client);
    }
    return result;
}

bool BasicServerHandler::send_to_client(const ClientPtr & client, const sihd::util::IArray & arr)
{
    if (!client)
        return false;
    std::lock_guard lock(_mutex);
    auto it = _client_map.find(client->fd());
    if (it == _client_map.end() || it->second != client)
        return false;
    client->write_array.byte_resize(arr.byte_size());
    if (client->write_array.copy_from_bytes(arr))
        return this->server()->add_client_write(client->fd());
    return false;
}

bool BasicServerHandler::remove_client(const ClientPtr & client)
{
    if (!client)
        return false;
    std::lock_guard lock(_mutex);
    auto it = _client_map.find(client->fd());
    if (it == _client_map.end() || it->second != client)
        return false;
    int fd = client->fd();
    client->disconnected = true;
    this->server()->remove_client_read(fd);
    this->server()->remove_client_write(fd);
    _client_map.erase(it);
    return true;
}

bool BasicServerHandler::send_to_client(int socket, const sihd::util::IArray & arr)
{
    std::lock_guard lock(_mutex);
    auto it = _client_map.find(socket);
    if (it == _client_map.end())
        return false;
    return this->send_to_client(it->second, arr);
}

bool BasicServerHandler::remove_client(int socket)
{
    std::lock_guard lock(_mutex);
    auto it = _client_map.find(socket);
    if (it == _client_map.end())
        return false;
    return this->remove_client(it->second);
}

void BasicServerHandler::handle_no_activity([[maybe_unused]] INetServer *server, sihd::util::time::UnixTime milliseconds)
{
    if (_last_time <= 0)
        _last_time = _clock.now() + sihd::util::Duration(milliseconds);
    this->_reset();
    this->_add_time_to_clients();
    _poll_time = milliseconds;
}

void BasicServerHandler::handle_activity([[maybe_unused]] INetServer *server, sihd::util::time::UnixTime milliseconds)
{
    if (_last_time <= 0)
        _last_time = _clock.now() + sihd::util::Duration(milliseconds);
    this->_reset();
    this->_add_time_to_clients();
    _poll_time = milliseconds;
}

void BasicServerHandler::handle_new_client(INetServer *server)
{
    IpAddr addr;
    int socket = server->accept_client(&addr);
    if (socket >= 0)
    {
        std::lock_guard lock(_mutex);
        if (_client_map.size() >= _max_clients)
        {
#if !defined(__SIHD_WINDOWS__)
            ::close(socket);
#else
            ::closesocket(socket);
#endif
            return;
        }
        auto client = std::make_shared<Client>(socket);
        client->read_array.reserve(4096);
        client->write_array.reserve(4096);
        client->addr = addr;
        client->time_connected = _clock.now();
        server->add_client_read(socket);
        _client_map[socket] = client;
        _connect_event_lst.push_back(client);
    }
}

void BasicServerHandler::handle_client_read(INetServer *server, int socket)
{
    std::lock_guard lock(_mutex);
    auto it = _client_map.find(socket);
    if (it != _client_map.end())
    {
        auto & client = it->second;
        ssize_t rcv;
        {
            std::lock_guard lk(client->mutex);
            rcv = client->socket.receive(client->read_array);
        }
        client->error = (rcv < 0);
        client->disconnected = rcv == 0;
        if (rcv <= 0)
            server->remove_client_read(socket);
        _read_event_lst.push_back(client);
    }
}

void BasicServerHandler::handle_client_write(INetServer *server, int socket)
{
    std::lock_guard lock(_mutex);
    auto it = _client_map.find(socket);
    if (it != _client_map.end())
    {
        auto & client = it->second;
        bool success = client->socket.send_all(client->write_array);
        client->error = !success;
        _write_event_lst.push_back(client);
        server->remove_client_write(socket);
    }
}

void BasicServerHandler::handle_after_activity(INetServer *server)
{
    _server = server;
    bool had_activity;
    {
        std::lock_guard lock(_mutex);
        had_activity = !_connect_event_lst.empty() || !_read_event_lst.empty() || !_write_event_lst.empty();
    }
    // only notify on real activity (idle poll timeouts would otherwise spam observers)
    if (had_activity)
        this->notify_observers(this);
}

} // namespace sihd::net
