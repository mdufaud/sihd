#include <sihd/net/LoggerReaderUdp.hpp>
#include <sihd/net/LoggerRecord.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(LoggerReaderUdp)

SIHD_LOGGER;

using namespace sihd::util;

LoggerReaderUdp::LoggerReaderUdp(const std::string & name, Node *parent):
    Named(name, parent),
    _receiver("udp-receiver"),
    _buffer(max_record_size)
{
    _receiver.add_observer(this);
    this->add_conf("host", &LoggerReaderUdp::set_host);
    this->add_conf("port", &LoggerReaderUdp::set_port);
    this->add_conf("unix_path", &LoggerReaderUdp::set_unix_path);
}

LoggerReaderUdp::~LoggerReaderUdp()
{
    if (this->is_running())
        this->stop();
}

bool LoggerReaderUdp::set_host(std::string_view host)
{
    return !SIHD_UNEXPECTED_LOG(_endpoint.set_host(host));
}

bool LoggerReaderUdp::set_port(int port)
{
    return !SIHD_UNEXPECTED_LOG(_endpoint.set_port(port));
}

bool LoggerReaderUdp::set_unix_path(std::string_view path)
{
    return !SIHD_UNEXPECTED_LOG(_endpoint.set_unix_path(path));
}

Result<int> LoggerReaderUdp::local_port() const
{
    return _receiver.socket().local_port();
}

bool LoggerReaderUdp::is_ready() const
{
    return _receiver.is_ready();
}

bool LoggerReaderUdp::wait_ready(Duration timeout) const
{
    return _receiver.wait_ready(timeout);
}

bool LoggerReaderUdp::on_start()
{
    if (_endpoint.unix_path().empty() && _endpoint.host().empty())
    {
        SIHD_LOG(error, "{}: host or unix_path must be set", this->name());
        return false;
    }
    auto bound = _endpoint.unix_path().empty() ? _receiver.open_and_bind(_endpoint.ip_addr())
                                               : _receiver.open_unix_and_bind(_endpoint.unix_path());
    if (SIHD_UNEXPECTED_LOG(bound))
    {
        (void)_receiver.close();
        return false;
    }
    return _receiver.start();
}

bool LoggerReaderUdp::on_stop()
{
    _receiver.stop();
    (void)_receiver.close();
    return true;
}

void LoggerReaderUdp::handle(INetReceiver *receiver)
{
    auto received = receiver->receive(_buffer);
    if (!received)
    {
        if (received.error().retryable() == false)
            SIHD_LOG(warning, "{}: {}", this->name(), received.error().message);
        return;
    }
    LoggerRecord record;
    const Result<void> decoded = record.decode(_buffer);
    if (decoded)
        LoggerManager::log(record.log_info(), record.message());
    else
        SIHD_LOG(warning, "{}: {}", this->name(), decoded.error().message);
}

} // namespace sihd::net
