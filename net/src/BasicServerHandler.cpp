#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/util/Logger.hpp>
#include <unistd.h>

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

BasicServerHandler::~BasicServerHandler()
{
    for (Client *client : _client_lst)
    {
        delete client;
    }
}

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
    time_t t = _clock.now() - _last_time;
    for (Client *client : _client_lst)
    {
        client->time_total += t;
    }
}

bool BasicServerHandler::set_max_clients(size_t max)
{
    _max_clients = max;
    return true;
}

bool BasicServerHandler::send_to_client(Client *client, const sihd::util::IArray & arr)
{
    if (client != nullptr && client->write_array.copy_from_bytes(arr))
    {
        client->write_array.resize(arr.byte_size());
        return this->server()->add_client_write(client->fd());
    }
    return false;
}

bool BasicServerHandler::remove_client(Client *client)
{
    if (client != nullptr)
    {
        auto it = std::find(_client_lst.begin(), _client_lst.end(), client);
        if (it != _client_lst.end())
            _client_lst.erase(it);
        this->server()->remove_client_read(client->fd());
        this->server()->remove_client_write(client->fd());
        delete client;
    }
    return client != nullptr;
}

bool BasicServerHandler::send_to_client(int socket, const sihd::util::IArray & arr)
{
    if (_client_map.find(socket) == _client_map.end())
        return false;
    return this->send_to_client(_client_map[socket], arr);
}

bool BasicServerHandler::remove_client(int socket)
{
    if (_client_map.find(socket) == _client_map.end())
        return false;
    return this->remove_client(_client_map[socket]);
}

void BasicServerHandler::handle_no_activity([[maybe_unused]] INetServer *server, time_t nano)
{
    if (_last_time <= 0)
        _last_time = _clock.now() + nano;
    this->_reset();
    this->_add_time_to_clients();
    _poll_time = nano;
}

void BasicServerHandler::handle_activity([[maybe_unused]] INetServer *server, time_t nano)
{
    if (_last_time <= 0)
        _last_time = _clock.now() + nano;
    this->_reset();
    this->_add_time_to_clients();
    _poll_time = nano;
}

void BasicServerHandler::handle_new_client(INetServer *server)
{
    IpAddr addr;
    int socket = server->accept_client(&addr);
    if (socket >= 0)
    {
        if (_client_lst.size() >= _max_clients)
        {
            close(socket);
            return;
        }
        Client *client = new Client(socket);
        // add client to internal listing
        _client_map[socket] = client;
        _client_lst.push_back(client);
        // fill client
        client->read_array.reserve(4096);
        client->write_array.reserve(4096);
        client->addr = addr;
        client->time_connected = _clock.now();
        // add client to new events
        _connect_event_lst.push_back(client);
        // add client socket to poll reading
        server->add_client_read(socket);
    }
}

void BasicServerHandler::handle_client_read(INetServer *server, int socket)
{
    (void)server;
    Client *client = _client_map[socket];
    if (client != nullptr)
    {
        ssize_t rcv = client->socket.receive(client->read_array);
        client->error = (rcv < 0);
        client->disconnected = rcv == 0;
        _read_event_lst.push_back(client);
    }
}

void BasicServerHandler::handle_client_write(INetServer *server, int socket)
{
    (void)server;
    Client *client = _client_map[socket];
    if (client != nullptr)
    {
        bool success = client->socket.send_all(client->write_array);
        client->error = !success;
        _write_event_lst.push_back(client);
        server->remove_client_write(socket);
    }
}

void BasicServerHandler::handle_after_activity(INetServer *server)
{
    _server = server;
    this->notify_observers(this);
}

} // namespace sihd::net