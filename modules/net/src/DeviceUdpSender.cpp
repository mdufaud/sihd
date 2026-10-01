#include <sihd/net/DeviceUdpSender.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(DeviceUdpSender)

SIHD_LOGGER;

DeviceUdpSender::DeviceUdpSender(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _udp_sender("udp-sender"),
    _port(0),
    _channel_tx(nullptr)
{
    this->add_conf("host", &DeviceUdpSender::set_host);
    this->add_conf("port", &DeviceUdpSender::set_port);
    this->add_conf("unix_path", &DeviceUdpSender::set_unix_path);
}

DeviceUdpSender::~DeviceUdpSender() = default;

bool DeviceUdpSender::is_running() const
{
    return _udp_sender.socket().is_open();
}

bool DeviceUdpSender::set_host(std::string_view host)
{
    _host = host;
    return true;
}

bool DeviceUdpSender::set_port(int port)
{
    _port = port;
    return true;
}

bool DeviceUdpSender::set_unix_path(std::string_view path)
{
    _unix_path = path;
    return true;
}

bool DeviceUdpSender::on_init()
{
    this->add_unlinked_channel("tx", sihd::util::Type::TYPE_BYTE, 1);
    return true;
}

bool DeviceUdpSender::on_start()
{
    _channel_tx = this->get_channel("tx").value_or(nullptr);
    if (_channel_tx == nullptr)
        return false;

    if (_unix_path.empty() && (_host.empty() || _port <= 0))
    {
        SIHD_LOG(error, "DeviceUdpSender: host+port or unix_path must be set");
        return false;
    }

    _channel_tx->set_resizable(true);
    _channel_tx->reserve(4096);

    this->observe_channel(_channel_tx);

    auto connected = !_unix_path.empty() ? _udp_sender.open_unix_and_connect(_unix_path) //
                                         : _udp_sender.open_and_connect(_host, _port);
    if (!connected)
    {
        SIHD_LOG(error, "DeviceUdpSender: failed to connect");
        return false;
    }

    return true;
}

bool DeviceUdpSender::on_stop()
{
    (void)_udp_sender.close();
    return true;
}

bool DeviceUdpSender::on_reset()
{
    _channel_tx = nullptr;
    return true;
}

void DeviceUdpSender::handle(sihd::core::Channel *c)
{
    if (c == _channel_tx)
    {
        SIHD_UNEXPECTED_LOG(_udp_sender.send_all(sihd::util::ArrCharView(*c->array())));
    }
}

} // namespace sihd::net
