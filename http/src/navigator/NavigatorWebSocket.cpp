#include <cstring>

#include <sihd/http/scheme.hpp>
#include <sihd/util/Logger.hpp>

#include "NavigatorImpl.hpp"

namespace sihd::http
{

using sihd::util::Url;

SIHD_LOGGER;

int Navigator::Impl::ws_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
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
                std::lock_guard lock(impl->ws.mutex);
                impl->ws.connected    = true;
                impl->ws.handshake_done = true;
            }
            impl->ws.cv.notify_all();

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
                        sihd::util::ArrByteView view(static_cast<const uint8_t *>(in), len);
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
            std::lock_guard lock(impl->ws.mutex);
            if (!impl->ws.send_queue.empty())
            {
                auto & msg      = impl->ws.send_queue.front();
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
            SIHD_LOG(error, "Navigator: WebSocket connection error: {}", error_msg);
            {
                std::lock_guard lock(impl->ws.mutex);
                impl->ws.connected    = false;
                impl->ws.handshake_done = true;
            }
            impl->ws.cv.notify_all();
            break;
        }
        case LWS_CALLBACK_CLIENT_CLOSED:
        {
            SIHD_LOG(debug, "Navigator: WebSocket connection closed");
            {
                std::lock_guard lock(impl->ws.mutex);
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

bool Navigator::Impl::ws_do_connect(std::string_view url, std::string_view protocol)
{
    Url parsed(url);

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

    ws.context = lws_create_context(&ctx_info);
    if (ws.context == nullptr)
    {
        SIHD_LOG(error, "Navigator: failed to create lws context for WebSocket client");
        return false;
    }

    struct lws_client_connect_info connect_info;
    memset(&connect_info, 0, sizeof(connect_info));
    connect_info.context   = ws.context;
    connect_info.address = parsed.host.c_str();
    connect_info.port = parsed.port != 0 ? parsed.port : scheme_port(parsed.scheme);
    connect_info.path = parsed.path.c_str();
    connect_info.host = parsed.host.c_str();
    connect_info.origin = parsed.host.c_str();
    connect_info.protocol = protocol_str.c_str();
    connect_info.userdata = this;
    connect_info.ietf_version_or_minus_one = -1;

    if (scheme_is_ssl(parsed.scheme))
    {
        int ssl_flags = LCCSCF_USE_SSL;
        if (!http.ssl_verify_peer)
            ssl_flags |= LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
        connect_info.ssl_connection = ssl_flags;
    }

    ws.wsi = lws_client_connect_via_info(&connect_info);
    if (ws.wsi == nullptr)
    {
        SIHD_LOG(error, "Navigator: failed to initiate WebSocket connection");
        lws_context_destroy(ws.context);
        ws.context = nullptr;
        return false;
    }

    ws.handshake_done = false;
    ws.connected      = false;

    ws.worker.set_method([this] {
        while (!ws.stop_requested && ws.worker.is_worker_started())
        {
            lws_service(ws.context, 0);
            {
                std::lock_guard lock(ws.mutex);
                if (!ws.connected && ws.handshake_done)
                    return true;
            }
        }
        return true;
    });
    ws.worker.start_worker("navigator-ws");

    {
        std::unique_lock lock(ws.mutex);
        ws.cv.wait_for(lock, std::chrono::seconds(http.timeout_s), [this] { return ws.handshake_done; });
    }

    if (!ws.connected)
        ws_disconnect();

    return ws.connected;
}

void Navigator::Impl::ws_disconnect()
{
    ws.stop_requested = true;
    if (ws.context != nullptr)
        lws_cancel_service(ws.context);
    ws.worker.stop_worker();

    if (ws.context != nullptr)
    {
        lws_context_destroy(ws.context);
        ws.context = nullptr;
    }
    ws.wsi            = nullptr;
    ws.connected      = false;
    ws.handshake_done = false;
    ws.stop_requested = false;
}

bool Navigator::Impl::ws_queue_message(const void *data, size_t len, bool binary)
{
    std::lock_guard lock(ws.mutex);
    if (!ws.connected || ws.wsi == nullptr)
        return false;

    WsState::Message msg;
    msg.data.assign(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + len);
    msg.binary = binary;
    ws.send_queue.push(std::move(msg));

    lws_callback_on_writable(ws.wsi);
    lws_cancel_service(ws.context);

    return true;
}

} // namespace sihd::http
