#include <sihd/net/DevUdpReceiver.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

using namespace sihd::util;

namespace sihd::net
{

SIHD_REGISTER_FACTORY(DevUdpReceiver)

SIHD_LOGGER;

DevUdpReceiver::DevUdpReceiver(const std::string & name, Node *parent):
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
    this->add_conf("host", &DevUdpReceiver::set_host);
    this->add_conf("port", &DevUdpReceiver::set_port);
    this->add_conf("buffer_capacity", &DevUdpReceiver::set_buffer_capacity);
    this->add_conf("poll_timeout", &DevUdpReceiver::set_poll_timeout);
}

DevUdpReceiver::~DevUdpReceiver()
{
    if (this->is_running())
        this->stop();
}

bool DevUdpReceiver::is_running() const
{
    return _worker.is_worker_running();
}

bool DevUdpReceiver::set_host(std::string_view host)
{
    _host = host;
    return true;
}

bool DevUdpReceiver::set_port(int port)
{
    _port = port;
    return true;
}

bool DevUdpReceiver::set_buffer_capacity(size_t capacity)
{
    _buffer_capacity = capacity;
    return true;
}

bool DevUdpReceiver::set_poll_timeout(int milliseconds)
{
    return _udp_receiver.set_poll_timeout(milliseconds);
}

bool DevUdpReceiver::on_init()
{
    this->add_unlinked_channel_resizable("rx", Type::TYPE_BYTE, 1, _buffer_capacity);
    return true;
}

bool DevUdpReceiver::on_start()
{
    auto rx = this->get_channel("rx");
    if (SIHD_UNEXPECTED_LOG(rx))
        return false;
    _channel_rx = *rx;

    if (_port <= 0)
    {
        SIHD_LOG(error, "DevUdpReceiver: port must be set");
        return false;
    }

    _stop_requested = false;
    _start_ok = false;

    if (!_worker.start_worker("DevUdpReceiver"))
        return false;

    _start_sync.sync();
    return _start_ok;
}

bool DevUdpReceiver::on_stop()
{
    _stop_requested = true;
    _udp_receiver.stop();
    _worker.stop_worker();
    _udp_receiver.remove_observer(this);
    (void)_udp_receiver.close();
    return true;
}

bool DevUdpReceiver::on_reset()
{
    _channel_rx = nullptr;
    return true;
}

bool DevUdpReceiver::run()
{
    IpAddr addr(_host.empty() ? "0.0.0.0" : _host, _port);
    auto bound = _udp_receiver.open_and_bind(addr);

    if (SIHD_UNEXPECTED_LOG(bound))
    {
        _start_ok = false;
        _start_sync.sync();
        return false;
    }

    _udp_receiver.add_observer(this);

    _start_ok = true;
    _start_sync.sync();

    if (_udp_receiver.start() == false)
        SIHD_LOG(error, "DevUdpReceiver: could not start the receiver");

    return true;
}

void DevUdpReceiver::handle([[maybe_unused]] sihd::core::Channel *c) {}

void DevUdpReceiver::handle(INetReceiver *receiver)
{
    ArrByte buf(_buffer_capacity);
    auto received = receiver->receive(buf);
    if (SIHD_UNEXPECTED_LOG(received))
        return;
    if (received.value() > 0)
    {
        buf.resize(received.value());
        _channel_rx->write(buf);
    }
}

} // namespace sihd::net
