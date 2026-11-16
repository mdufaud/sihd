#include <sihd/http/AWebsocketHandler.hpp>

namespace sihd::http
{

using namespace sihd::util;

AWebsocketHandler::AWebsocketHandler(IWebsocketConnection & connection): _connection(connection) {}

AWebsocketHandler::~AWebsocketHandler() = default;

Result<void> AWebsocketHandler::send(std::string_view text)
{
    return _connection.send(text);
}

Result<void> AWebsocketHandler::send_binary(ArrByteView data)
{
    return _connection.send_binary(data);
}

void AWebsocketHandler::close()
{
    _connection.close();
}

bool AWebsocketHandler::is_connected() const
{
    return _connection.is_connected();
}

size_t AWebsocketHandler::dropped() const
{
    return _connection.dropped();
}

std::string_view AWebsocketHandler::protocol_name() const
{
    return _connection.protocol_name();
}

const HttpRequest & AWebsocketHandler::request() const
{
    return _connection.request();
}

bool AWebsocketHandler::on_auth(HttpRequest &)
{
    return true;
}

void AWebsocketHandler::on_open() {}

void AWebsocketHandler::on_binary(ArrByteView) {}

void AWebsocketHandler::on_peer_close(uint16_t, std::string_view) {}

void AWebsocketHandler::on_close() {}

} // namespace sihd::http
