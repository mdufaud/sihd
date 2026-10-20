#include <cstring>

#include <sihd/http/scheme.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include "NavigatorImpl.hpp"

using namespace sihd::util;

namespace sihd::http
{

SIHD_LOGGER;

void Navigator::Impl::WsState::LwsContextFree::operator()(struct lws_context *context) const
{
    lws_context_destroy(context);
}

int Navigator::Impl::ws_lws_callback(struct lws *wsi,
                                     enum lws_callback_reasons reason,
                                     void *user,
                                     void *in,
                                     size_t len)
{
    Navigator::Impl *impl = static_cast<Navigator::Impl *>(user);
    if (impl == nullptr)
        return 0;

    switch (reason)
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            SIHD_LOG(debug, "Navigator: WebSocket connection established");
            {
                auto lock = impl->ws.waitable.guard();
                impl->ws.connected = true;
                impl->ws.handshake_done = true;
            }
            impl->ws.waitable.notify_all();

            const lws_protocols *proto = lws_get_protocol(wsi);
            if (impl->owner->on_ws_open && proto != nullptr)
                impl->owner->on_ws_open(proto->name);

            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE:
        {
            if (in != nullptr && len > 0)
            {
                bool is_binary = lws_frame_is_binary(wsi);
                if (is_binary)
                {
                    if (impl->owner->on_ws_binary)
                    {
                        ArrByteView view(static_cast<const uint8_t *>(in), len);
                        impl->owner->on_ws_binary(view);
                    }
                }
                else
                {
                    if (impl->owner->on_ws_text)
                    {
                        std::string_view text(static_cast<const char *>(in), len);
                        impl->owner->on_ws_text(text);
                    }
                }
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_WRITEABLE:
        {
            auto lock = impl->ws.waitable.guard();
            if (!impl->ws.send_queue.empty())
            {
                auto & msg = impl->ws.send_queue.front();
                size_t data_len = msg.data.size();

                std::vector<uint8_t> buf(LWS_PRE + data_len);
                std::memcpy(buf.data() + LWS_PRE, msg.data.data(), data_len);

                enum lws_write_protocol proto = msg.binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT;
                int written = lws_write(wsi, buf.data() + LWS_PRE, data_len, proto);

                impl->ws.send_queue.pop();

                if (written < 0)
                {
                    SIHD_LOG(error, "Navigator: WebSocket write failed");
                    return -1;
                }

                if (!impl->ws.send_queue.empty())
                    lws_callback_on_writable(wsi);
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            const char *error_msg = (in != nullptr) ? static_cast<const char *>(in) : "unknown error";
            {
                auto lock = impl->ws.waitable.guard();
                impl->ws.connect_error = error_msg;
                impl->ws.connected = false;
                impl->ws.handshake_done = true;
            }
            impl->ws.waitable.notify_all();
            break;
        }
        case LWS_CALLBACK_CLIENT_CLOSED:
        {
            SIHD_LOG(debug, "Navigator: WebSocket connection closed");
            {
                auto lock = impl->ws.waitable.guard();
                impl->ws.connected = false;
            }
            if (impl->owner->on_ws_close)
                impl->owner->on_ws_close();
            break;
        }
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
        {
            uint16_t close_code = 0;
            std::string_view close_reason;
            if (len >= 2)
            {
                close_code = static_cast<uint16_t>((static_cast<uint8_t *>(in)[0] << 8)
                                                   | static_cast<uint8_t *>(in)[1]);
                if (len > 2)
                    close_reason = std::string_view(static_cast<const char *>(in) + 2, len - 2);
            }
            if (impl->owner->on_ws_peer_close)
                impl->owner->on_ws_peer_close(close_code, close_reason);
            break;
        }
        default:
            break;
    }

    return 0;
}

Result<std::string> Navigator::Impl::ws_proxy_address()
{
    if (proxy.disabled)
        return std::string();

    auto selected = select_proxy();
    if (!selected.has_value())
        return std::string();

    const auto & [pu, pt] = *selected;
    if (pt != ProxyType::Http)
        return std::unexpected(Error(ErrorCode::not_supported, "Navigator: WebSocket only supports an HTTP proxy"));

    std::string host = pu;
    if (host.rfind("http://", 0) == 0)
        host.erase(0, 7);
    else if (host.rfind("https://", 0) == 0)
        host.erase(0, 8);

    if (!proxy.user.empty() && !proxy.pass.empty())
        return proxy.user + ":" + proxy.pass + "@" + host;
    return host;
}

Result<void> Navigator::Impl::ws_do_connect(std::string_view url, std::string_view protocol)
{
    Url parsed(url);
    str::to_lower(parsed.scheme);
    if (parsed.scheme != "ws" && parsed.scheme != "wss" && parsed.scheme != "http" && parsed.scheme != "https")
    {
        return std::unexpected(
            Error(ErrorCode::invalid_argument, "Navigator: unsupported WebSocket URL scheme: {}", url));
    }
    std::string connect_address = parsed.host;
    if (ssrf_guard)
    {
        auto vetted = sihd::net::ip::public_addresses(parsed.host);
        if (!vetted)
        {
            return std::unexpected(Error(ErrorCode::permission_denied,
                                         "SSRF guard blocked a WebSocket connection to a private host: {}",
                                         url));
        }
        // lws would resolve the name again: the vetted address closes the rebinding window
        connect_address = vetted->front();
    }

    // the parsed path has lost the query: keep the raw one, a fragment never reaches the server
    std::string request_path = parsed.path.empty() ? "/" : parsed.path;
    const std::string_view without_fragment = url.substr(0, url.find('#'));
    if (const size_t query_start = without_fragment.find('?'); query_start != std::string_view::npos)
        request_path += without_fragment.substr(query_start);

    std::string protocol_str(protocol);
    struct lws_protocols protocols[] = {
        {protocol_str.c_str(), ws_lws_callback, 0, 4096, 0, this, 0},
        LWS_PROTOCOL_LIST_TERM,
    };

    struct lws_context_creation_info ctx_info;
    memset(&ctx_info, 0, sizeof(ctx_info));
    ctx_info.port = CONTEXT_PORT_NO_LISTEN;
    ctx_info.protocols = protocols;
    ctx_info.gid = -1;
    ctx_info.uid = -1;
    if (scheme_is_ssl(parsed.scheme))
        ctx_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    // Apply Navigator proxy config: lws otherwise silently uses the http_proxy env var.
    // A non-null http_proxy_address disables that env-var fallback ("" means no proxy).
    auto ws_proxy_addr = ws_proxy_address();
    SIHD_UNEXPECTED_RETURN(ws_proxy_addr);
    if (proxy.disabled || !ws_proxy_addr->empty())
        ctx_info.http_proxy_address = ws_proxy_addr->c_str();
    // else: leave http_proxy_address null so lws uses the env proxy

    ws.context.reset(lws_create_context(&ctx_info));
    if (ws.context == nullptr)
        return std::unexpected(
            Error(ErrorCode::unknown, "Navigator: failed to create lws context for WebSocket client"));

    struct lws_client_connect_info connect_info;
    memset(&connect_info, 0, sizeof(connect_info));
    connect_info.context = ws.context.get();
    connect_info.address = connect_address.c_str();
    connect_info.port = parsed.port != 0 ? parsed.port : scheme_port(parsed.scheme);
    connect_info.path = request_path.c_str();
    connect_info.host = parsed.host.c_str();
    connect_info.origin = parsed.host.c_str();
    connect_info.protocol = protocol_str.c_str();
    connect_info.userdata = this;
    connect_info.ietf_version_or_minus_one = -1;

    if (scheme_is_ssl(parsed.scheme))
    {
        int ssl_flags = LCCSCF_USE_SSL;
        if (!options.ssl_verify_peer)
            ssl_flags |= LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
        connect_info.ssl_connection = ssl_flags;
    }

    ws.wsi = lws_client_connect_via_info(&connect_info);
    if (ws.wsi == nullptr)
    {
        ws.context.reset();
        return std::unexpected(Error(ErrorCode::io_error, "Navigator: failed to initiate WebSocket connection"));
    }

    ws.handshake_done = false;
    ws.connected = false;
    ws.connect_error.clear();

    ws.worker.set_method([this] {
        while (!ws.stop_requested && ws.worker.is_worker_started())
        {
            lws_service(ws.context.get(), 0);
            {
                auto lock = ws.waitable.guard();
                if (!ws.connected && ws.handshake_done)
                    return true;
            }
        }
        return true;
    });
    ws.worker.start_worker("navigator-ws");

    ws.waitable.wait_for(options.timeout, [this] { return ws.handshake_done; });

    if (ws.connected)
        return {};

    std::string reason;
    {
        auto lock = ws.waitable.guard();
        reason = ws.connect_error.empty() ? "handshake did not complete" : ws.connect_error;
    }
    ws_disconnect();
    return std::unexpected(Error(ErrorCode::io_error, "Navigator: WebSocket connection failed: {}", reason));
}

void Navigator::Impl::ws_disconnect()
{
    ws.stop_requested = true;
    if (ws.context != nullptr)
        lws_cancel_service(ws.context.get());
    ws.worker.stop_worker();

    ws.context.reset();
    ws.wsi = nullptr;
    ws.connected = false;
    ws.handshake_done = false;
    ws.stop_requested = false;
}

bool Navigator::Impl::ws_queue_message(const void *data, size_t len, bool binary)
{
    auto lock = ws.waitable.guard();
    if (!ws.connected || ws.wsi == nullptr)
        return false;

    WsState::Message msg;
    msg.data.assign(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + len);
    msg.binary = binary;
    ws.send_queue.push(std::move(msg));

    lws_callback_on_writable(ws.wsi);
    lws_cancel_service(ws.context.get());

    return true;
}

} // namespace sihd::http
