#include <fmt/format.h>

#include <sihd/net/LoggerEndpoint.hpp>

namespace sihd::net
{

using namespace sihd::util;

LoggerEndpoint::LoggerEndpoint(): _port(0) {}

Result<void> LoggerEndpoint::set_host(std::string_view host)
{
    if (_unix_path.empty() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "host and unix_path are exclusive"));
    if (IpAddr(host).has_ip() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "host is not an ip: '{}'", host));
    _host = host;
    return {};
}

Result<void> LoggerEndpoint::set_port(int port)
{
    if (_unix_path.empty() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "port and unix_path are exclusive"));
    if (port < 0 || port > 65535)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid port {}", port));
    _port = port;
    return {};
}

Result<void> LoggerEndpoint::set_unix_path(std::string_view path)
{
    if (_host.empty() == false || _port != 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "unix_path and host/port are exclusive"));
    if (path.empty())
        return std::unexpected(Error(ErrorCode::invalid_argument, "empty unix_path"));
    _unix_path = path;
    return {};
}

const std::string & LoggerEndpoint::host() const
{
    return _host;
}

int LoggerEndpoint::port() const
{
    return _port;
}

const std::string & LoggerEndpoint::unix_path() const
{
    return _unix_path;
}

IpAddr LoggerEndpoint::ip_addr() const
{
    return IpAddr(_host, _port);
}

std::string LoggerEndpoint::str() const
{
    return _unix_path.empty() ? fmt::format("{}:{}", _host, _port) : _unix_path;
}

} // namespace sihd::net
