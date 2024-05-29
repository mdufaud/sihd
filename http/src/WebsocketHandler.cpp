#include <sihd/util/Logger.hpp>

#include <sihd/http/WebsocketHandler.hpp>

namespace sihd::http
{

SIHD_LOGGER;

WebsocketHandler::WebsocketHandler() {}

WebsocketHandler::~WebsocketHandler() {}

void WebsocketHandler::on_open(std::string_view protocol_name)
{
    if (this->callback_open)
        this->callback_open(protocol_name);
}

bool WebsocketHandler::on_read(const sihd::util::ArrChar & array)
{
    return !this->callback_read || this->callback_read(array);
}

bool WebsocketHandler::on_write(sihd::util::ArrChar & array, LwsWriteProtocol & protocol)
{
    return !this->callback_write || this->callback_write(array, protocol);
}

void WebsocketHandler::on_close()
{
    if (this->callback_close)
        this->callback_close();
}

} // namespace sihd::http