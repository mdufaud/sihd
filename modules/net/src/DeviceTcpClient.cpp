#include <sihd/net/DeviceTcpClient.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(DeviceTcpClient)

SIHD_LOGGER;

DeviceTcpClient::DeviceTcpClient(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _tcp_client("tcp-client"),
    _start_sync(2),
    _stop_requested(false),
    _start_ok(false),
    _port(0),
    _buffer_capacity(4096),
    _connect_timeout(Socket::blocking_timeout),
    _reconnect_interval(1000),
    _channel_rx(nullptr),
    _channel_tx(nullptr),
    _channel_connected(nullptr)
{
    // windows shutdown does not wake a blocking recv (linux does): bound the
    // rx drain or the stop join parks forever on a quiet socket
    if constexpr (sihd::util::build::is_windows)
        _tcp_client.set_recv_timeout(100);
    _worker.set_runnable(this);
    this->add_conf("host", &DeviceTcpClient::set_host);
    this->add_conf("port", &DeviceTcpClient::set_port);
    this->add_conf("unix_path", &DeviceTcpClient::set_unix_path);
    this->add_conf("buffer_capacity", &DeviceTcpClient::set_buffer_capacity);
    this->add_conf("poll_timeout", &DeviceTcpClient::set_poll_timeout);
    this->add_conf("connect_timeout", &DeviceTcpClient::set_connect_timeout);
    this->add_conf("reconnect_interval", &DeviceTcpClient::set_reconnect_interval);
}

DeviceTcpClient::~DeviceTcpClient()
{
    if (this->is_running())
        this->stop();
}

bool DeviceTcpClient::is_running() const
{
    return _worker.is_worker_running();
}

bool DeviceTcpClient::set_host(std::string_view host)
{
    _host = host;
    return true;
}

bool DeviceTcpClient::set_port(int port)
{
    _port = port;
    return true;
}

bool DeviceTcpClient::set_unix_path(std::string_view path)
{
    _unix_path = path;
    return true;
}

bool DeviceTcpClient::set_buffer_capacity(size_t capacity)
{
    _buffer_capacity = capacity;
    return true;
}

bool DeviceTcpClient::set_poll_timeout(int milliseconds)
{
    return _tcp_client.set_poll_timeout(milliseconds);
}

bool DeviceTcpClient::set_connect_timeout(int milliseconds)
{
    _connect_timeout = milliseconds;
    return true;
}

bool DeviceTcpClient::set_reconnect_interval(int milliseconds)
{
    _reconnect_interval = milliseconds;
    return true;
}

bool DeviceTcpClient::on_init()
{
    this->add_unlinked_channel_resizable("rx", sihd::util::Type::TYPE_BYTE, 1, _buffer_capacity);
    this->add_unlinked_channel_resizable("tx", sihd::util::Type::TYPE_BYTE, 1, _buffer_capacity);
    this->add_unlinked_channel("connected", sihd::util::Type::TYPE_BOOL, 1);
    return true;
}

bool DeviceTcpClient::on_start()
{
    _channel_rx = this->get_channel("rx").value_or(nullptr);
    _channel_tx = this->get_channel("tx").value_or(nullptr);
    _channel_connected = this->get_channel("connected").value_or(nullptr);

    if (_channel_rx == nullptr || _channel_tx == nullptr || _channel_connected == nullptr)
        return false;

    if (_unix_path.empty() && (_host.empty() || _port <= 0))
    {
        SIHD_LOG(error, "DeviceTcpClient: host+port or unix_path must be set");
        return false;
    }

    this->observe_channel(_channel_tx);

    _stop_requested = false;
    _start_ok = false;

    if (!_worker.start_worker("DeviceTcpClient"))
        return false;

    _start_sync.sync();
    return _start_ok;
}

bool DeviceTcpClient::on_stop()
{
    _stop_requested = true;
    _waitable.notify_all();
    // best-effort unblock of a stuck tx send: the join below waits for it
    (void)_tcp_client.socket().shutdown();
    _worker.stop_worker();
    _tcp_client.remove_observer(this);
    (void)_tcp_client.close();

    this->_set_connected(false);

    return true;
}

bool DeviceTcpClient::on_reset()
{
    _channel_rx = nullptr;
    _channel_tx = nullptr;
    _channel_connected = nullptr;
    return true;
}

std::expected<void, sihd::util::Error> DeviceTcpClient::_connect()
{
    if (!_unix_path.empty())
        return _tcp_client.open_unix_and_connect(_unix_path);
    return _tcp_client.open_and_connect(_host, _port, _connect_timeout);
}

void DeviceTcpClient::_set_connected(bool connected)
{
    if (_channel_connected != nullptr)
        _channel_connected->write<bool>(0, connected);
}

bool DeviceTcpClient::run()
{
    auto connected = this->_connect();
    if (connected)
    {
        this->_set_connected(true);
        _tcp_client.add_observer(this);
    }
    else
    {
        SIHD_LOG(error, "DeviceTcpClient: failed to connect");
    }

    _start_ok = connected.has_value();
    _start_sync.sync();

    if (!connected)
        return false;

    while (!_stop_requested)
    {
        while (!_stop_requested && _tcp_client.connected())
        {
            if (_tcp_client.socket().tls_pending())
                this->handle(&_tcp_client);
            else
                _tcp_client.poll();
        }
        if (_stop_requested)
            break;

        _tcp_client.remove_observer(this);
        this->_set_connected(false);
        (void)_tcp_client.close();

        SIHD_LOG(info, "DeviceTcpClient: disconnected, reconnecting...");

        while (!_stop_requested)
        {
            _waitable.wait_for(std::chrono::milliseconds(_reconnect_interval),
                               [this] { return this->_stop_requested.load(); });
            if (_stop_requested)
                break;

            if (this->_connect())
            {
                SIHD_LOG(info, "DeviceTcpClient: reconnected");
                this->_set_connected(true);
                _tcp_client.add_observer(this);
                break;
            }
            // a failed connect leaves the socket open: the next attempt would fail on open
            (void)_tcp_client.close();
        }
    }

    return true;
}

void DeviceTcpClient::handle(sihd::core::Channel *c)
{
    if (c != _channel_tx)
        return;
    if (_tcp_client.connected())
        SIHD_UNEXPECTED_LOG(_tcp_client.send_all(sihd::util::ArrCharView(*c->array())));
}

void DeviceTcpClient::handle(INetReceiver *receiver)
{
    if (!_tcp_client.connected())
    {
        this->_set_connected(false);
        return;
    }

    sihd::util::ArrByte buf(_buffer_capacity);
    // notifications must drain: the peer FIN notification is the last one
    while (true)
    {
        auto received = receiver->receive(buf);
        if (!received || received.value() <= 0)
            break;
        _channel_rx->write(buf);
    }
    if (!_tcp_client.connected())
        this->_set_connected(false);
}

} // namespace sihd::net
