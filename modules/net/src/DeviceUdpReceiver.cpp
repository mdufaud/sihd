#include <sihd/net/DeviceUdpReceiver.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(DeviceUdpReceiver)

SIHD_LOGGER;

DeviceUdpReceiver::DeviceUdpReceiver(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _udp_receiver("udp-receiver"),
    _start_sync(2),
    _stop_requested(false),
    _start_ok(false),
    _port(0),
    _buffer_capacity(4096),
    _channel_rx(nullptr)
{
    _worker.set_runnable(this);
    this->add_conf("host", &DeviceUdpReceiver::set_host);
    this->add_conf("port", &DeviceUdpReceiver::set_port);
    this->add_conf("buffer_capacity", &DeviceUdpReceiver::set_buffer_capacity);
    this->add_conf("poll_timeout", &DeviceUdpReceiver::set_poll_timeout);
}

DeviceUdpReceiver::~DeviceUdpReceiver()
{
    if (this->is_running())
        this->stop();
}

bool DeviceUdpReceiver::is_running() const
{
    return _worker.is_worker_running();
}

bool DeviceUdpReceiver::set_host(std::string_view host)
{
    _host = host;
    return true;
}

bool DeviceUdpReceiver::set_port(int port)
{
    _port = port;
    return true;
}

bool DeviceUdpReceiver::set_buffer_capacity(size_t capacity)
{
    _buffer_capacity = capacity;
    return true;
}

bool DeviceUdpReceiver::set_poll_timeout(int milliseconds)
{
    return _udp_receiver.set_poll_timeout(milliseconds);
}

bool DeviceUdpReceiver::on_init()
{
    this->add_unlinked_channel_resizable("rx", sihd::util::Type::TYPE_BYTE, 1, _buffer_capacity);
    return true;
}

bool DeviceUdpReceiver::on_start()
{
    _channel_rx = this->get_channel("rx");
    if (_channel_rx == nullptr)
        return false;

    if (_port <= 0)
    {
        SIHD_LOG(error, "DeviceUdpReceiver: port must be set");
        return false;
    }

    _stop_requested = false;
    _start_ok = false;

    if (!_worker.start_worker("DeviceUdpReceiver"))
        return false;

    _start_sync.sync();
    return _start_ok;
}

bool DeviceUdpReceiver::on_stop()
{
    _stop_requested = true;
    _udp_receiver.stop();
    _worker.stop_worker();
    _udp_receiver.remove_observer(this);
    _udp_receiver.close();
    return true;
}

bool DeviceUdpReceiver::on_reset()
{
    _channel_rx = nullptr;
    return true;
}

bool DeviceUdpReceiver::run()
{
    IpAddr addr(_host.empty() ? "0.0.0.0" : _host, _port);
    bool ok = _udp_receiver.open_and_bind(addr);

    if (!ok)
    {
        SIHD_LOG(error, "DeviceUdpReceiver: failed to bind on {}:{}", _host, _port);
        _start_ok = false;
        _start_sync.sync();
        return false;
    }

    _udp_receiver.add_observer(this);

    _start_ok = true;
    _start_sync.sync();

    _udp_receiver.start();

    return true;
}

void DeviceUdpReceiver::handle([[maybe_unused]] sihd::core::Channel *c) {}

void DeviceUdpReceiver::handle(INetReceiver *receiver)
{
    sihd::util::ArrByte buf(_buffer_capacity);
    ssize_t received = receiver->receive(buf);
    if (received > 0)
    {
        buf.resize(static_cast<size_t>(received));
        _channel_rx->write(buf);
    }
}

} // namespace sihd::net
