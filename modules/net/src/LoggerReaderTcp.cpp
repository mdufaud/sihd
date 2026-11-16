#include <sihd/net/LoggerReaderTcp.hpp>
#include <sihd/net/LoggerRecord.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/endian.hpp>

namespace sihd::net
{

SIHD_REGISTER_FACTORY(LoggerReaderTcp)

SIHD_LOGGER;

using namespace sihd::util;

LoggerReaderTcp::LoggerReaderTcp(const std::string & name, Node *parent): Named(name, parent), _server("tcp-server")
{
    _server.set_server_handler(&_handler);
    _server.set_service_wait_stop(true);
    _handler.add_observer(this);
    this->add_conf("host", &LoggerReaderTcp::set_host);
    this->add_conf("port", &LoggerReaderTcp::set_port);
    this->add_conf("unix_path", &LoggerReaderTcp::set_unix_path);
}

LoggerReaderTcp::~LoggerReaderTcp()
{
    if (this->is_running())
        this->stop();
}

bool LoggerReaderTcp::set_host(std::string_view host)
{
    return !SIHD_UNEXPECTED_LOG(_endpoint.set_host(host));
}

bool LoggerReaderTcp::set_port(int port)
{
    return !SIHD_UNEXPECTED_LOG(_endpoint.set_port(port));
}

bool LoggerReaderTcp::set_unix_path(std::string_view path)
{
    return !SIHD_UNEXPECTED_LOG(_endpoint.set_unix_path(path));
}

Result<int> LoggerReaderTcp::local_port() const
{
    return _server.socket().local_port();
}

bool LoggerReaderTcp::is_ready() const
{
    return _server.is_ready();
}

bool LoggerReaderTcp::wait_ready(Duration timeout) const
{
    return _server.wait_ready(timeout);
}

bool LoggerReaderTcp::on_start()
{
    if (_endpoint.unix_path().empty() && _endpoint.host().empty())
    {
        SIHD_LOG(error, "{}: host or unix_path must be set", this->name());
        return false;
    }
    auto bound = _endpoint.unix_path().empty() ? _server.open_and_bind(_endpoint.ip_addr())
                                               : _server.open_unix_and_bind(_endpoint.unix_path());
    if (SIHD_UNEXPECTED_LOG(bound))
    {
        (void)_server.close();
        return false;
    }
    return _server.start();
}

bool LoggerReaderTcp::on_stop()
{
    _server.stop();
    this->_close_clients();
    (void)_server.close();
    return true;
}

void LoggerReaderTcp::_close_clients()
{
    for (const BasicServerHandler::ClientPtr & client : _handler.clients())
        _handler.remove_client(client);
    _pending.clear();
}

void LoggerReaderTcp::handle(BasicServerHandler *handler)
{
    for (const BasicServerHandler::ClientPtr & client : handler->read_activity())
    {
        const bool framed = this->_read_records(client);
        if (framed == false || client->disconnected || client->error)
        {
            handler->remove_client(client);
            _pending.erase(client->id);
        }
    }
}

bool LoggerReaderTcp::_read_records(const BasicServerHandler::ClientPtr & client)
{
    std::string & pending = _pending[client->id];
    pending.append(reinterpret_cast<const char *>(client->read_array.buf()), client->read_array.byte_size());

    size_t offset = 0;
    uint32_t size = 0;
    while (pending.size() - offset >= sizeof(size))
    {
        size = endian::read_be<uint32_t>(reinterpret_cast<const uint8_t *>(pending.data()) + offset, sizeof(size));
        if (size > max_record_size)
        {
            SIHD_LOG(warning, "{}: refused a record of {} bytes, dropping its client", this->name(), size);
            return false;
        }
        if (pending.size() - offset - sizeof(size) < size)
            break;

        LoggerRecord record;
        const Result<void> decoded = record.decode(ArrByteView(pending.data() + offset + sizeof(size), size));
        if (decoded)
            LoggerManager::log(record.log_info(), record.message());
        else
            SIHD_LOG(warning, "{}: {}", this->name(), decoded.error().message);
        offset += sizeof(size) + size;
    }

    pending.erase(0, offset);
    return true;
}

} // namespace sihd::net
