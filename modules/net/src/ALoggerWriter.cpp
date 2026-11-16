#include <fmt/format.h>

#include <sihd/net/ALoggerWriter.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::net
{

using namespace sihd::util;

SIHD_LOGGER;

ALoggerWriter::ALoggerWriter(const std::string & name, Node *parent, int socket_type):
    ALogger(name, parent),
    _socket_type(socket_type),
    _reconnect_interval(1000),
    _connect_timeout(1000),
    _send_timeout(1000),
    _connected(false),
    _warned_unconfigured(false),
    _warned_outage(false)
{
    this->add_conf("host", &ALoggerWriter::set_host);
    this->add_conf("port", &ALoggerWriter::set_port);
    this->add_conf("unix_path", &ALoggerWriter::set_unix_path);
    this->add_conf("reconnect_interval", &ALoggerWriter::set_reconnect_interval);
    this->add_conf("connect_timeout", &ALoggerWriter::set_connect_timeout);
    this->add_conf("send_timeout", &ALoggerWriter::set_send_timeout);
}

ALoggerWriter::~ALoggerWriter() = default;

bool ALoggerWriter::set_host(std::string_view host)
{
    return this->_endpoint_changed(_endpoint.set_host(host));
}

bool ALoggerWriter::set_port(int port)
{
    return this->_endpoint_changed(_endpoint.set_port(port));
}

bool ALoggerWriter::set_unix_path(std::string_view path)
{
    return this->_endpoint_changed(_endpoint.set_unix_path(path));
}

bool ALoggerWriter::set_reconnect_interval(int milliseconds)
{
    if (milliseconds < 0)
    {
        SIHD_LOG(error, "{}: reconnect interval cannot be negative: {}", this->name(), milliseconds);
        return false;
    }
    _reconnect_interval = milliseconds;
    return true;
}

bool ALoggerWriter::set_connect_timeout(int milliseconds)
{
    if (milliseconds <= 0)
    {
        SIHD_LOG(error, "{}: connect timeout must be positive: {}", this->name(), milliseconds);
        return false;
    }
    _connect_timeout = milliseconds;
    return true;
}

bool ALoggerWriter::set_send_timeout(int milliseconds)
{
    if (milliseconds <= 0)
    {
        SIHD_LOG(error, "{}: send timeout must be positive: {}", this->name(), milliseconds);
        return false;
    }
    _send_timeout = milliseconds;
    return true;
}

bool ALoggerWriter::is_connected() const
{
    return _connected;
}

bool ALoggerWriter::_endpoint_changed(const Result<void> & changed)
{
    if (SIHD_UNEXPECTED_LOG(changed))
        return false;
    this->_establish_at_conf();
    return true;
}

bool ALoggerWriter::_is_configured() const
{
    return _endpoint.unix_path().empty() == false || (_endpoint.host().empty() == false && _endpoint.port() != 0);
}

void ALoggerWriter::_establish_at_conf()
{
    this->_disconnect();
    _failed_at.reset();
    _warned_outage = false;
    if (this->_is_configured() == false)
        return;
    auto connected = this->_connect();
    if (connected)
        return;
    _failed_at = _clock.now();
    _warned_outage = true;
    SIHD_LOG(warning,
             "{}: cannot reach {}: {}, retrying when logging",
             this->name(),
             _endpoint.str(),
             connected.error().message);
}

bool ALoggerWriter::_establish()
{
    if (_failed_at.has_value() && _clock.now() - *_failed_at < Duration(time::ms(_reconnect_interval)))
        return false;
    auto connected = this->_connect();
    if (!connected)
    {
        _failed_at = _clock.now();
        this->_warn_outage(connected.error().message);
        return false;
    }
    _failed_at.reset();
    _warned_outage = false;
    return true;
}

Result<void> ALoggerWriter::_open_socket()
{
    const int domain = _endpoint.unix_path().empty() ? (_endpoint.ip_addr().is_ipv6() ? AF_INET6 : AF_INET) : AF_UNIX;
    if (auto opened = _socket.open(domain, _socket_type, 0); !opened)
        return opened;
    return _socket.set_send_timeout(_send_timeout);
}

Result<void> ALoggerWriter::_connect()
{
    this->_disconnect();
    auto connected = this->_open_socket();
    if (connected)
    {
        connected = _endpoint.unix_path().empty() ? _socket.connect(_endpoint.ip_addr(), _connect_timeout)
                                                  : _socket.connect_unix(_endpoint.unix_path());
    }
    if (!connected)
    {
        (void)_socket.close();
        return connected;
    }
    _connected = true;
    return {};
}

void ALoggerWriter::_disconnect()
{
    _connected = false;
    if (_socket.is_open())
        (void)_socket.close();
}

void ALoggerWriter::_warn_outage(std::string_view reason)
{
    if (_warned_outage)
        return;
    _warned_outage = true;
    LoggerManager::sink_warning(
        this->name(),
        fmt::format("cannot reach {}: {}, dropping messages until it is back", _endpoint.str(), reason));
}

void ALoggerWriter::log(const LogInfo & info, std::string_view msg)
{
    if (this->_is_configured() == false)
    {
        if (_warned_unconfigured == false)
        {
            _warned_unconfigured = true;
            LoggerManager::sink_warning(this->name(), "no host/port nor unix_path configured, dropping messages");
        }
        return;
    }
    if (_connected == false && this->_establish() == false)
        return;
    auto sent = _socket.send_all(this->encode_frame(LoggerRecord(info, msg)));
    if (!sent)
    {
        this->_disconnect();
        _failed_at = _clock.now();
        this->_warn_outage(sent.error().message);
    }
}

} // namespace sihd::net
