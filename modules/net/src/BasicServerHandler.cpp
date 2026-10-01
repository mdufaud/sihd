#include <algorithm>

#include <sihd/net/BasicServerHandler.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

SIHD_LOGGER;

namespace
{

constexpr size_t max_read_per_poll = 64 * 1024;
constexpr size_t max_read_array = max_read_per_poll * 2;

} // namespace

BasicServerHandler::BasicServerHandler()
{
    _last_time = sihd::util::Timestamp(0);
    _poll_time = sihd::util::Duration(0);
    _server = nullptr;
    this->set_max_clients(512);
    this->set_tls_accept_timeout(5000);
    this->add_conf("max_clients", &BasicServerHandler::set_max_clients);
    this->add_conf("tls_accept_timeout", &BasicServerHandler::set_tls_accept_timeout);
}

BasicServerHandler::~BasicServerHandler() = default;

void BasicServerHandler::set_tls_context(sihd::crypto::TlsContext ctx)
{
    _tls_ctx = std::move(ctx);
}

bool BasicServerHandler::set_tls_accept_timeout(int milliseconds)
{
    // a negative timeout would run SSL_accept unbounded
    if (milliseconds < 0)
        return false;
    _tls_accept_timeout = milliseconds;
    return true;
}

INetServer *BasicServerHandler::server()
{
    std::lock_guard lock(_mutex);
    return _server;
}

void BasicServerHandler::_reset()
{
    std::lock_guard lock(_mutex);
    _read_event_lst.clear();
    _write_event_lst.clear();
    _connect_event_lst.clear();
}

void BasicServerHandler::_add_time_to_clients()
{
    if (_last_time <= 0)
        return;
    std::lock_guard lock(_mutex);
    const auto now = _clock.now();
    const sihd::util::Duration t = now - _last_time;
    if (t <= 0)
        return;
    for (auto & [fd, client] : _client_map)
    {
        client->time_total += t;
    }
    _last_time = now;
}

bool BasicServerHandler::set_max_clients(size_t max)
{
    _max_clients = max;
    return true;
}

bool BasicServerHandler::_client_limit_reached() const
{
    // callers hold _mutex
    return _client_map.size() >= _max_clients;
}

size_t BasicServerHandler::client_count() const
{
    // handshaking clients are not served yet: only ready ones count
    std::lock_guard lock(_mutex);
    size_t count = 0;
    for (const auto & [fd, client] : _client_map)
    {
        count += client->state == Client::State::ready;
    }
    return count;
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
    {
        std::lock_guard lk(client->mutex);
        // a pending partial write holds unsent bytes: refuse rather than drop
        if (client->write_offset < client->write_array.byte_size())
            return false;
        if (!client->write_array.byte_resize(arr.byte_size()))
            return false;
        if (!client->write_array.copy_from_bytes(arr))
            return false;
        client->write_offset = 0;
    }
    // no registration means no POLLOUT to drain: hand the buffer back
    if (_server == nullptr || this->server()->add_client_write(client->fd()) == false)
    {
        std::lock_guard lk(client->mutex);
        client->write_array.byte_resize(0);
        client->write_offset = 0;
        return false;
    }
    return true;
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
    if (_server != nullptr)
    {
        this->server()->remove_client_read(fd);
        this->server()->remove_client_write(fd);
    }
    {
        std::lock_guard lk(client->mutex);
        client->disconnected = true;
        (void)client->socket.close();
    }
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

void BasicServerHandler::handle_no_activity(INetServer *server, sihd::util::time::UnixTime milliseconds)
{
    this->handle_activity(server, milliseconds);
}

void BasicServerHandler::handle_activity(INetServer *server, sihd::util::time::UnixTime milliseconds)
{
    if (_last_time <= 0)
        _last_time = _clock.now() + sihd::util::Duration(milliseconds);
    this->_reset();
    this->_add_time_to_clients();
    this->_expire_tls_handshakes(server, _steady_clock.now());
    _poll_time = sihd::util::Duration(milliseconds);
}

void BasicServerHandler::handle_new_client(INetServer *server)
{
    IpAddr addr;
    auto accepted = server->accept_client(&addr);
    if (SIHD_UNEXPECTED_LOG(accepted))
        return;
    const int socket = accepted.value();
    {
        std::lock_guard lock(_mutex);
        if (this->_client_limit_reached())
        {
            (void)Socket::close_socket(socket);
            return;
        }
    }
    auto client = std::make_shared<Client>(socket);
    client->read_array.reserve(4096);
    client->write_array.reserve(4096);
    client->addr = addr;
    client->time_connected = _clock.now();
    if (_tls_ctx)
    {
        client->socket.set_tls_context(*_tls_ctx);
        const TlsHandshakeStep step = client->socket.tls_accept_step();
        if (step == TlsHandshakeStep::failed)
            return;
        if (step != TlsHandshakeStep::complete)
        {
            client->state = Client::State::handshaking;
            client->handshake_step = step;
            client->handshake_deadline = _steady_clock.now()
                                         + sihd::util::Duration(sihd::util::time::ms(_tls_accept_timeout));
            std::lock_guard lock(_mutex);
            if (this->_client_limit_reached())
                return;
            const bool registered = step == TlsHandshakeStep::want_read ? server->add_client_read(socket)
                                                                        : server->add_client_write(socket);
            if (!registered)
            {
                SIHD_LOG(error, "BasicServerHandler: cannot poll TLS handshake");
                return;
            }
            _client_map.emplace(socket, client);
            return;
        }
    }
    if (!client->socket.set_blocking(false))
    {
        SIHD_LOG(error, "BasicServerHandler: cannot set client socket non-blocking");
        return;
    }
    std::lock_guard lock(_mutex);
    if (this->_client_limit_reached())
        return;
    if (!server->add_client_read(socket))
    {
        SIHD_LOG(error, "BasicServerHandler: cannot poll more clients");
        return;
    }
    _client_map.emplace(socket, client);
    _connect_event_lst.push_back(client);
}

void BasicServerHandler::_advance_tls_handshake(INetServer *server,
                                                ClientMap::iterator it,
                                                bool readable,
                                                bool writable)
{
    // callers hold _mutex
    ClientPtr client = it->second;
    const int socket = client->fd();
    if ((client->handshake_step == TlsHandshakeStep::want_read && !readable)
        || (client->handshake_step == TlsHandshakeStep::want_write && !writable))
        return;

    const TlsHandshakeStep step = client->socket.tls_accept_step();
    if (step == TlsHandshakeStep::failed)
    {
        this->_drop_client(server, it);
        return;
    }
    if (step == TlsHandshakeStep::complete)
    {
        server->remove_client_write(socket);
        server->remove_client_read(socket);
        if (!server->add_client_read(socket))
        {
            this->_drop_client(server, it);
            SIHD_LOG(error, "BasicServerHandler: cannot poll TLS client");
            return;
        }
        client->state = Client::State::ready;
        _connect_event_lst.push_back(client);
        return;
    }

    client->handshake_step = step;
    const bool registered = step == TlsHandshakeStep::want_read ? server->add_client_read(socket)
                                                                : server->add_client_write(socket);
    if (!registered)
    {
        this->_drop_client(server, it);
        SIHD_LOG(error, "BasicServerHandler: cannot poll TLS handshake");
        return;
    }
    if (step == TlsHandshakeStep::want_read)
        server->remove_client_write(socket);
    else
        server->remove_client_read(socket);
}

BasicServerHandler::ClientMap::iterator BasicServerHandler::_drop_client(INetServer *server, ClientMap::iterator it)
{
    // callers hold _mutex
    server->remove_client_read(it->first);
    server->remove_client_write(it->first);
    std::lock_guard lk(it->second->mutex);
    (void)it->second->socket.close();
    return _client_map.erase(it);
}

void BasicServerHandler::_expire_tls_handshakes(INetServer *server, sihd::util::Timestamp now)
{
    std::lock_guard lock(_mutex);
    for (auto it = _client_map.begin(); it != _client_map.end();)
    {
        if (it->second->state != Client::State::handshaking || now < it->second->handshake_deadline)
        {
            ++it;
            continue;
        }
        it = this->_drop_client(server, it);
    }
}

void BasicServerHandler::handle_client_read(INetServer *server, int socket)
{
    std::unique_lock map_lock(_mutex);
    auto it = _client_map.find(socket);
    if (it == _client_map.end())
        return;
    if (it->second->state == Client::State::handshaking)
    {
        this->_advance_tls_handshake(server, it, true, false);
        return;
    }
    ClientPtr client = it->second;
    std::unique_lock client_lock(client->mutex);
    map_lock.unlock();

    client->read_array.byte_resize(0);
    size_t total_read = 0;
    // SSL-buffered bytes never re-fire poll: drain them past the poll budget
    while (total_read < max_read_per_poll || client->socket.tls_pending())
    {
        const size_t size = client->read_array.byte_size();
        if (size == client->read_array.byte_capacity())
        {
            const size_t next_capacity = std::min(max_read_array,
                                                  std::max<size_t>(client->read_array.byte_capacity() * 2, 4096));
            if (next_capacity == client->read_array.byte_capacity() || !client->read_array.byte_reserve(next_capacity))
                break;
        }
        const size_t budget = total_read < max_read_per_poll ? max_read_per_poll - total_read
                                                             : client->read_array.byte_capacity() - size;
        const size_t available = std::min(client->read_array.byte_capacity() - size, budget);
        const auto more = client->socket.receive(client->read_array.buf() + size, available);
        if (!more)
        {
            client->error = !more.error().retryable();
            break;
        }
        if (more.value() == 0)
        {
            client->disconnected = true;
            break;
        }
        if (!client->read_array.byte_resize(size + more.value()))
        {
            client->error = true;
            break;
        }
        total_read += more.value();
    }
    client_lock.unlock();

    map_lock.lock();
    it = _client_map.find(socket);
    if (it == _client_map.end() || it->second != client)
        return;
    if (client->error || client->disconnected)
    {
        this->_drop_client(server, it);
    }
    else if (total_read == 0)
    {
        return;
    }
    _read_event_lst.push_back(client);
}

void BasicServerHandler::handle_client_write(INetServer *server, int socket)
{
    std::unique_lock map_lock(_mutex);
    auto it = _client_map.find(socket);
    if (it == _client_map.end())
        return;
    if (it->second->state == Client::State::handshaking)
    {
        this->_advance_tls_handshake(server, it, false, true);
        return;
    }
    // keep the client alive across the map erase below
    ClientPtr client = it->second;
    bool fully_sent = false;
    {
        std::lock_guard lk(client->mutex);
        const size_t remaining = client->write_array.byte_size() - client->write_offset;
        if (remaining == 0)
        {
            fully_sent = true;
        }
        else
        {
            const auto sent = client->socket.send(
                {(char *)client->write_array.buf() + client->write_offset, remaining});
            if (!sent && sent.error().retryable())
                return;
            if (sent)
                client->write_offset += sent.value();
            // a zero send with bytes left would spin a level-triggered poll
            client->error = !sent || sent.value() == 0;
            fully_sent = !client->error && client->write_offset >= client->write_array.byte_size();
        }
    }
    if (client->error)
    {
        this->_drop_client(server, it);
    }
    else if (fully_sent)
    {
        server->remove_client_write(socket);
    }
    _write_event_lst.push_back(client);
}

void BasicServerHandler::handle_after_activity(INetServer *server)
{
    bool had_activity;
    {
        std::lock_guard lock(_mutex);
        _server = server;
        had_activity = !_connect_event_lst.empty() || !_read_event_lst.empty() || !_write_event_lst.empty();
    }
    // only notify on real activity (idle poll timeouts would otherwise spam observers)
    if (had_activity)
        this->notify_observers(this);
}

} // namespace sihd::net
