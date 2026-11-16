#include <array>
#include <exception>
#include <memory>
#include <optional>

#include <sihd/http/AWebsocketHandler.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Url.hpp>

#include "../WriteProtocol.hpp"
#include "HttpServerImpl.hpp"
#include "WebsocketConnection.hpp"

namespace sihd::http
{

SIHD_LOGGER;

using namespace sihd::util;

int HttpServer::Impl::_global_websocket_lws_callback(struct lws *wsi,
                                                     enum lws_callback_reasons reason,
                                                     void *user,
                                                     void *in,
                                                     size_t len)
{
    HttpServer *srv = (HttpServer *)lws_context_user(lws_get_context(wsi));
    // the handler hooks run in the lws callback frame: an exception would terminate
    try
    {
        return srv->_impl->_lws_websocket_callback(wsi, reason, user, in, len);
    }
    catch (const std::exception & error)
    {
        SIHD_LOG(error, "HttpServer: websocket handler threw: {}", error.what());
        return -1;
    }
}

int HttpServer::Impl::_lws_websocket_callback(struct lws *wsi,
                                              enum lws_callback_reasons reason,
                                              void *user,
                                              void *in,
                                              size_t len)
{
    WebsocketSession **slot = (WebsocketSession **)user;
    WebsocketSession *session = slot != nullptr ? *slot : nullptr;
    int rc = 0;
    switch (reason)
    {
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
        {
            rc = (slot != nullptr && this->open_websocket_session(wsi, slot)) ? 0 : 1;
            break;
        }
        case LWS_CALLBACK_ESTABLISHED:
        {
            if (session != nullptr)
            {
                session->opened = true;
                session->handler->on_open();
                // a message queued before the switch found no writable connection to wake
                if (session->connection->has_pending() || session->connection->close_requested())
                    lws_callback_on_writable(wsi);
            }
            break;
        }
        case LWS_CALLBACK_SERVER_WRITEABLE:
        {
            if (session != nullptr && session->opened)
                rc = this->on_websocket_write(wsi, *session->connection);
            break;
        }
        case LWS_CALLBACK_RECEIVE:
        {
            if (session != nullptr && session->opened)
                rc = this->receive_websocket_message(wsi, *session, (const char *)in, len);
            break;
        }
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
        {
            if (session != nullptr && session->opened)
            {
                uint16_t close_code = 0;
                std::string_view close_reason;
                if (len >= 2)
                {
                    close_code = (uint16_t)(((uint8_t *)in)[0] << 8 | ((uint8_t *)in)[1]);
                    if (len > 2)
                        close_reason = std::string_view((const char *)in + 2, len - 2);
                }
                session->handler->on_peer_close(close_code, close_reason);
            }
            break;
        }
        case LWS_CALLBACK_WS_SERVER_DROP_PROTOCOL:
        {
            // the last callback where lws still owns the session memory: CLOSED comes after it
            if (slot != nullptr)
                this->close_websocket_session(slot);
            break;
        }
        case LWS_CALLBACK_CLOSED:
        {
            std::lock_guard lock(websocket_messages_mutex);
            websocket_messages.erase(wsi);
            break;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            const char *error = (const char *)in;
            if (error != nullptr)
                SIHD_LOG(warning, "HttpServer: client connection error: {}", error);
            break;
        }
        case LWS_CALLBACK_RECEIVE_PONG:
        {
            SIHD_LOG(debug, "HttpServer: received pong callback");
            break;
        }
        default:
            break;
    }
    return rc;
}

bool HttpServer::Impl::open_websocket_session(struct lws *wsi, WebsocketSession **slot)
{
    std::optional<HttpRequest> request = this->make_upgrade_request(wsi);
    if (request.has_value() == false || this->allow_websocket_upgrade(wsi, *request) == false)
        return false;

    const lws_protocols *protocol = lws_get_protocol(wsi);
    auto session = std::make_unique<WebsocketSession>();
    session->connection = std::make_unique<WebsocketConnection>(lws_get_context(wsi),
                                                                protocol->name,
                                                                websocket_max_pending,
                                                                std::move(*request));
    session->handler = websocket_factories[protocol->id](*session->connection);
    if (session->handler->on_auth(session->connection->request()) == false)
    {
        SIHD_LOG(warning,
                 "HttpServer: websocket upgrade of '{}' refused by its handler",
                 session->connection->request().url());
        return false;
    }
    *slot = session.release();
    return true;
}

void HttpServer::Impl::close_websocket_session(WebsocketSession **slot)
{
    std::unique_ptr<WebsocketSession> session(*slot);
    *slot = nullptr;
    if (session == nullptr)
        return;
    session->connection->disconnect();
    if (session->opened)
        session->handler->on_close();
}

int HttpServer::Impl::receive_websocket_message(struct lws *wsi, WebsocketSession & session, const char *in, size_t len)
{
    std::string message;
    const MessagePiece piece = this->gather_websocket_message(wsi, in, len, message);
    if (piece == MessagePiece::too_large)
    {
        lws_close_reason(wsi, LWS_CLOSE_STATUS_MESSAGE_TOO_LARGE, nullptr, 0);
        return -1;
    }
    if (piece == MessagePiece::complete)
    {
        if (lws_frame_is_binary(wsi))
            session.handler->on_binary(ArrByteView(message.data(), message.size()));
        else
            session.handler->on_text(message);
    }
    return 0;
}

std::optional<HttpRequest> HttpServer::Impl::make_upgrade_request(struct lws *wsi)
{
    std::array<char, SIHD_HTTP_URI_BUFSIZE + 1> uri;
    const int uri_len = lws_hdr_copy(wsi, uri.data(), SIHD_HTTP_URI_BUFSIZE, WSI_TOKEN_GET_URI);
    if (uri_len < 0)
    {
        SIHD_LOG(warning, "HttpServer: websocket upgrade refused, uri too long");
        return std::nullopt;
    }
    HttpRequest request(std::string(uri.data(), uri_len), this->get_uri_args(wsi), HttpRequest::Get);
    this->apply_request_headers(wsi, request);
    this->populate_request(wsi, request);
    return request;
}

bool HttpServer::Impl::allow_websocket_upgrade(struct lws *wsi, HttpRequest & request)
{
    if (this->pass_http_filter(wsi, request.url()) == false)
    {
        SIHD_LOG(warning, "HttpServer: websocket upgrade of '{}' refused by the filter", request.url());
        return false;
    }

    if (this->is_websocket_origin_allowed(wsi) == false)
    {
        SIHD_LOG(warning, "HttpServer: websocket upgrade refused, origin not allowed");
        return false;
    }

    if (http_authenticator != nullptr)
    {
        const std::optional<std::string> authorization = this->get_header(wsi, WSI_TOKEN_HTTP_AUTHORIZATION);
        AuthResult auth;
        if (authorization.has_value())
            auth = this->parse_authorization(*authorization, http_authenticator);
        if (auth.authorized == false)
        {
            SIHD_LOG(warning, "HttpServer: websocket upgrade refused, not authorized");
            return false;
        }
        if (auth.user.empty() == false)
            request.set_auth_user(auth.user);
        if (auth.token.empty() == false)
            request.set_auth_token(auth.token);
    }

    return true;
}

bool HttpServer::Impl::is_websocket_origin_allowed(struct lws *wsi)
{
    const std::optional<std::string> origin = this->get_header(wsi, WSI_TOKEN_ORIGIN);
    // a client without an Origin is not a browser: the cross-site risk is the browser's
    if (origin.has_value() == false || origin->empty())
        return true;

    const std::string lowered = str::to_lower_copy(*origin);
    if (websocket_origins.contains("*") || websocket_origins.contains(lowered))
        return true;

    const std::optional<std::string> host = this->get_header(wsi, WSI_TOKEN_HOST);
    const Url origin_url(lowered);
    if (host.has_value() == false || origin_url.has_authority == false)
        return false;
    const Url host_url("//" + *host);
    // a domain can be rebound to this server: only a literal address or localhost proves the page is ours
    const bool literal_host = sihd::net::ip::is_valid_ip(host_url.host) || str::iequals(host_url.host, "localhost");
    return literal_host && str::iequals(origin_url.host, host_url.host) && origin_url.port == host_url.port;
}

HttpServer::Impl::MessagePiece
    HttpServer::Impl::gather_websocket_message(struct lws *wsi, const char *in, size_t len, std::string & message)
{
    std::lock_guard lock(websocket_messages_mutex);
    std::string & gathered = websocket_messages[wsi];
    if (max_message_size > 0 && gathered.size() + len > max_message_size)
    {
        websocket_messages.erase(wsi);
        return MessagePiece::too_large;
    }
    gathered.append(in, len);

    if (!lws_is_final_fragment(wsi) || lws_remaining_packet_payload(wsi) > 0)
        return MessagePiece::partial;
    message = std::move(gathered);
    websocket_messages.erase(wsi);
    return MessagePiece::complete;
}

int HttpServer::Impl::on_websocket_write(struct lws *wsi, WebsocketConnection & connection)
{
    if (std::optional<WebsocketConnection::Message> message = connection.pop())
    {
        if (this->write_websocket_frame(wsi, message->data, message->protocol) == false)
            return -1;
        if (connection.has_pending() || connection.close_requested())
            lws_callback_on_writable(wsi);
        return 0;
    }
    if (connection.close_requested())
    {
        lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, nullptr, 0);
        return -1;
    }
    return 0;
}

bool HttpServer::Impl::write_websocket_frame(struct lws *wsi, std::string_view data, WriteProtocol protocol)
{
    ArrByte buffer;
    buffer.resize(data.size() + LWS_PRE);
    memcpy(buffer.data() + LWS_PRE, data.data(), data.size());
    const int wrote = lws_write(wsi,
                                (u_char *)(buffer.data() + LWS_PRE),
                                data.size(),
                                (enum lws_write_protocol)write_protocol_to_lws(protocol));
    return wrote >= (int)data.size();
}

} // namespace sihd::http
