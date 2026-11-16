#include "WebsocketConnection.hpp"

namespace sihd::http
{

using namespace sihd::util;

WebsocketConnection::WebsocketConnection(struct lws_context *context,
                                         const std::string & protocol_name,
                                         size_t max_pending,
                                         HttpRequest && request):
    _context(context),
    _protocol_name(protocol_name),
    _max_pending(max_pending),
    _request(std::move(request))
{
}

WebsocketConnection::~WebsocketConnection() = default;

Result<void> WebsocketConnection::send(std::string_view text)
{
    return this->_push(Message {std::string(text), WriteProtocol::Text});
}

Result<void> WebsocketConnection::send_binary(ArrByteView data)
{
    return this->_push(Message {std::string((const char *)data.data(), data.size()), WriteProtocol::Binary});
}

void WebsocketConnection::close()
{
    _close_requested = true;
    _pending.terminate();
    if (_connected)
        lws_cancel_service(_context);
}

bool WebsocketConnection::is_connected() const
{
    return _connected;
}

size_t WebsocketConnection::dropped() const
{
    return _dropped;
}

std::string_view WebsocketConnection::protocol_name() const
{
    return _protocol_name;
}

const HttpRequest & WebsocketConnection::request() const
{
    return _request;
}

HttpRequest & WebsocketConnection::request()
{
    return _request;
}

std::optional<WebsocketConnection::Message> WebsocketConnection::pop()
{
    // cleared before the pop: a push the pop misses wakes the service again
    _wake_requested = false;
    return _pending.try_pop();
}

bool WebsocketConnection::has_pending() const
{
    return _pending.empty() == false;
}

bool WebsocketConnection::close_requested() const
{
    return _close_requested;
}

void WebsocketConnection::disconnect()
{
    _connected = false;
    _pending.terminate();
    _pending.clear();
}

Result<void> WebsocketConnection::_push(Message && message)
{
    const bool accepting = _connected && _close_requested == false;
    if (accepting && _pending.push(std::move(message), _max_pending))
    {
        // lws is not thread safe: the service thread asks for the write on LWS_CALLBACK_EVENT_WAIT_CANCELLED
        if (_wake_requested.exchange(true) == false)
            lws_cancel_service(_context);
        return {};
    }
    // a close racing the push terminates the queue: the refusal is a drop only while still open
    if (_connected == false || _close_requested)
        return std::unexpected(Error(ErrorCode::closed, "websocket connection closed"));
    ++_dropped;
    return std::unexpected(Error(ErrorCode::would_block, "websocket send queue full at {} messages", _max_pending));
}

} // namespace sihd::http
