#include <array>

#include <sihd/http/IWebsocketHandler.hpp>
#include <sihd/http/WriteProtocol.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Url.hpp>

#include "HttpServerImpl.hpp"

namespace sihd::http
{

SIHD_LOGGER;

using namespace sihd::util;

int write_protocol_to_lws(WriteProtocol protocol);

int HttpServer::Impl::_global_websocket_lws_callback(struct lws *wsi,
                                                     enum lws_callback_reasons reason,
                                                     void *user,
                                                     void *in,
                                                     size_t len)
{
    HttpServer *srv = (HttpServer *)lws_context_user(lws_get_context(wsi));
    return srv->_impl->_lws_websocket_callback(wsi, reason, user, in, len);
}

int HttpServer::Impl::_lws_websocket_callback(struct lws *wsi,
                                              enum lws_callback_reasons reason,
                                              void *user,
                                              void *in,
                                              size_t len)
{
    int rc = 0;
    IWebsocketHandler *handler = nullptr;
    WebsocketSession *session = (WebsocketSession *)user;
    if (session != nullptr)
    {
        session->protocol = lws_get_protocol(wsi);
        session->wsi = wsi;
        session->in = in;
        session->len = len;
        handler = websocket_handler_lst[session->protocol->id];
    }
    switch (reason)
    {
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
        {
            rc = this->allow_websocket_upgrade(wsi) ? 0 : 1;
            break;
        }
        case LWS_CALLBACK_ESTABLISHED:
        {
            if (handler != nullptr && session != nullptr)
                handler->on_open(session->protocol->name);
            break;
        }
        case LWS_CALLBACK_SERVER_WRITEABLE:
        {
            if (handler != nullptr && session != nullptr)
                rc = this->on_websocket_write(handler, session);
            break;
        }
        case LWS_CALLBACK_RECEIVE:
        {
            if (handler != nullptr && session != nullptr)
            {
                ArrChar arr;
                arr.assign_bytes((uint8_t *)session->in, session->len);
                rc = handler->on_read(arr) ? 0 : -1;
                if (rc == 0)
                    lws_callback_on_writable(session->wsi);
            }
            break;
        }
        case LWS_CALLBACK_CLOSED:
        {
            if (handler != nullptr)
                handler->on_close();
            break;
        }
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
        {
            if (handler != nullptr)
            {
                uint16_t close_code = 0;
                std::string_view close_reason;
                if (len >= 2)
                {
                    close_code = (uint16_t)(((uint8_t *)in)[0] << 8 | ((uint8_t *)in)[1]);
                    if (len > 2)
                        close_reason = std::string_view((const char *)in + 2, len - 2);
                }
                handler->on_peer_close(close_code, close_reason);
            }
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

bool HttpServer::Impl::allow_websocket_upgrade(struct lws *wsi)
{
    std::array<char, SIHD_HTTP_URI_BUFSIZE + 1> uri;
    const int uri_len = lws_hdr_copy(wsi, uri.data(), SIHD_HTTP_URI_BUFSIZE, WSI_TOKEN_GET_URI);
    if (uri_len < 0)
    {
        SIHD_LOG(warning, "HttpServer: websocket upgrade refused, uri too long");
        return false;
    }

    if (this->pass_http_filter(wsi, std::string_view(uri.data(), uri_len)) == false)
    {
        SIHD_LOG(warning,
                 "HttpServer: websocket upgrade of '{}' refused by the filter",
                 std::string_view(uri.data(), uri_len));
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

        if (authorization.has_value() == false
            || this->parse_authorization(*authorization, http_authenticator).authorized == false)
        {
            SIHD_LOG(warning, "HttpServer: websocket upgrade refused, not authorized");
            return false;
        }
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

int HttpServer::Impl::on_websocket_write(IWebsocketHandler *handler, WebsocketSession *session)
{
    ArrChar arr_to_write;
    WriteProtocol lws_proto = WriteProtocol::Text;
    bool ret = handler->on_write(arr_to_write, lws_proto);
    if (ret && arr_to_write.size() > 0)
    {
        ArrByte buffer;
        buffer.resize(arr_to_write.size() + LWS_PRE);
        memcpy(buffer.data() + LWS_PRE, arr_to_write.data(), arr_to_write.size());
        int wrote = lws_write(session->wsi,
                              (u_char *)(buffer.data() + LWS_PRE),
                              arr_to_write.size(),
                              (enum lws_write_protocol)write_protocol_to_lws(lws_proto));
        if (wrote < (int)arr_to_write.size())
            ret = false;
    }
    return ret ? 0 : -1;
}

} // namespace sihd::http
