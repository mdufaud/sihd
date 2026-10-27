#include <sihd/net/DevUdpSender.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

using namespace sihd::util;

namespace sihd::net
{

SIHD_REGISTER_FACTORY(DevUdpSender)

SIHD_LOGGER;

DevUdpSender::DevUdpSender(const std::string & name, Node *parent):
    sihd::core::Device(name, parent),
    _udp_sender("udp-sender"),
    _port(0),
    _channel_tx(nullptr)
{
    this->add_conf("host", &DevUdpSender::set_host);
    this->add_conf("port", &DevUdpSender::set_port);
    this->add_conf("unix_path", &DevUdpSender::set_unix_path);
}

DevUdpSender::~DevUdpSender() = default;

bool DevUdpSender::is_running() const
{
    return _udp_sender.socket().is_open();
}

bool DevUdpSender::set_host(std::string_view host)
{
    _host = host;
    return true;
}

bool DevUdpSender::set_port(int port)
{
    _port = port;
    return true;
}

bool DevUdpSender::set_unix_path(std::string_view path)
{
    _unix_path = path;
    return true;
}

bool DevUdpSender::on_init()
{
    this->add_unlinked_channel("tx", Type::TYPE_BYTE, 1);
    return true;
}

bool DevUdpSender::on_start()
{
    auto tx = this->get_channel("tx");
    if (SIHD_UNEXPECTED_LOG(tx))
        return false;
    _channel_tx = *tx;

    if (_unix_path.empty() && (_host.empty() || _port <= 0))
    {
        SIHD_LOG(error, "DevUdpSender: host+port or unix_path must be set");
        return false;
    }

    _channel_tx->set_resizable(true);
    _channel_tx->reserve(4096);

    this->observe_channel(_channel_tx);

    auto connected = !_unix_path.empty() ? _udp_sender.open_unix_and_connect(_unix_path) //
                                         : _udp_sender.open_and_connect(_host, _port);
    if (SIHD_UNEXPECTED_LOG(connected))
        return false;

    return true;
}

bool DevUdpSender::on_stop()
{
    (void)_udp_sender.close();
    return true;
}

bool DevUdpSender::on_reset()
{
    _channel_tx = nullptr;
    return true;
}

void DevUdpSender::handle(sihd::core::Channel *c)
{
    if (c == _channel_tx)
    {
        SIHD_UNEXPECTED_LOG(_udp_sender.send_all(ArrCharView(*c->array())));
    }
}

} // namespace sihd::net
