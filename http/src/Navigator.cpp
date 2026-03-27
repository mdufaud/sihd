#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include <sihd/http/Navigator.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include "curl_global.hpp"
#include "lws.hpp"

namespace sihd::http
{

SIHD_NEW_LOGGER("sihd::http");

namespace
{

void init_first_curl()
{
    curl_init_once();
}

size_t content_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *content = static_cast<std::string *>(userdata);
    content->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t header_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *header = static_cast<HttpHeader *>(userdata);
    std::string_view header_str(ptr, size * nmemb);
    header->add_header_from_str(header_str);
    return size * nmemb;
}

} // namespace

struct Navigator::Impl
{
        // curl persistent handle
        CURL *curl_handle = nullptr;

        // config
        bool verbose = false;
        bool follow_redirects = true;
        long max_redirects = 10;
        long timeout_s = 30;
        bool ssl_verify_peer = false;
        bool ssl_verify_host = false;
        std::string user_agent;

        // auth
        std::string username;
        std::string password;
        std::string token;

        // persistent headers
        std::map<std::string, std::string> headers;

        // websocket state
        struct lws_context *ws_context = nullptr;
        struct lws *ws_wsi = nullptr;
        std::thread ws_thread;
        std::mutex ws_mutex;
        std::condition_variable ws_cv;
        bool ws_connected = false;
        bool ws_handshake_done = false;
        bool ws_should_stop = false;

        struct WsMessage
        {
                std::vector<uint8_t> data;
                bool binary;
        };
        std::queue<WsMessage> ws_send_queue;

        Navigator *owner = nullptr;

        Impl(Navigator *nav): owner(nav)
        {
            init_first_curl();
            curl_handle = curl_easy_init();
            if (curl_handle == nullptr)
                throw std::runtime_error("could not initialize curl");
            // enable in-memory cookie engine
            curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
        }

        ~Impl()
        {
            ws_disconnect();
            if (curl_handle != nullptr)
                curl_easy_cleanup(curl_handle);
        }

        void reset_handle_for_request()
        {
            // preserve cookies across handle reset
            std::map<std::string, std::string> saved = extract_cookies();

            curl_easy_reset(curl_handle);

            // re-enable cookie engine after reset
            curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");

            // restore cookies into the new handle
            for (const auto & [n, v] : saved)
            {
                std::string cookie_line = fmt::format(".\tTRUE\t/\tFALSE\t0\t{}\t{}", n, v);
                curl_easy_setopt(curl_handle, CURLOPT_COOKIELIST, cookie_line.c_str());
            }
        }

        curl_slist *build_headers()
        {
            curl_slist *slist = nullptr;

            // token auth via header
            if (!token.empty())
            {
                std::string auth = fmt::format("Authorization: Bearer {}", token);
                slist = curl_slist_append(slist, auth.c_str());
            }

            // persistent headers
            for (const auto & [name, value] : headers)
            {
                std::string h = fmt::format("{}: {}", name, value);
                slist = curl_slist_append(slist, h.c_str());
            }

            return slist;
        }

        void configure_handle(const std::string & url, HttpResponse & response, std::string & content)
        {
            curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, (long)verbose);
            curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());

            // response headers
            curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_write_callback);
            curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &response.http_header());

            // response body
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, content_write_callback);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &content);

            // config
            curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, timeout_s);
            curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, (long)follow_redirects);
            curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, max_redirects);
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, (long)ssl_verify_peer);
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, ssl_verify_host ? 2L : 0L);

            if (!user_agent.empty())
                curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, user_agent.c_str());

            // basic auth
            if (!username.empty() && !password.empty())
            {
                curl_easy_setopt(curl_handle, CURLOPT_USERNAME, username.c_str());
                curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, password.c_str());
            }
        }

        std::optional<NavigatorResponse> perform_request(const std::string & url)
        {
            HttpResponse response;
            std::string content;

            this->configure_handle(url, response, content);

            curl_slist *slist = this->build_headers();
            if (slist != nullptr)
                curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);

            CURLcode code = curl_easy_perform(curl_handle);

            if (slist != nullptr)
                curl_slist_free_all(slist);

            if (code != CURLE_OK)
            {
                SIHD_LOG(error, "Navigator: request failed: {}", curl_easy_strerror(code));
                return std::nullopt;
            }

            // build response
            int http_status = 0;
            curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_status);

            char *content_type = nullptr;
            curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &content_type);

            char *effective_url = nullptr;
            curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &effective_url);

            long redirect_count = 0;
            curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_COUNT, &redirect_count);

            response.set_status(http_status);
            if (content_type != nullptr)
                response.set_content_type(content_type);
            response.set_content(content);

            NavigatorResponse nav_response(std::move(response));

            if (effective_url != nullptr)
                nav_response.set_final_url(std::string(effective_url));

            auto cookies_map = this->extract_cookies();

            // also parse Set-Cookie response header (single-header case from curl)
            std::string_view sc = response.http_header().find("set-cookie:");
            if (!sc.empty())
            {
                auto [nv, _] = sihd::util::str::split_pair_view(sc, ";");
                auto [name, value] = sihd::util::str::split_pair_view(nv, "=");
                if (!name.empty())
                    cookies_map[std::string(name)] = std::string(value);
            }

            nav_response.set_cookies(std::move(cookies_map));

            // persist Cookie header for subsequent requests
            const auto & all_cookies = nav_response.cookies();
            if (!all_cookies.empty())
            {
                std::string cookie_hdr;
                for (const auto & [n, v] : all_cookies)
                {
                    if (!cookie_hdr.empty())
                        cookie_hdr += "; ";
                    cookie_hdr += n + "=" + v;
                }
                this->headers["Cookie"] = std::move(cookie_hdr);
            }

            // redirect history
            if (redirect_count > 0)
            {
                std::vector<std::string> history;
                history.push_back(url);
                if (effective_url != nullptr && std::string(effective_url) != url)
                    history.push_back(std::string(effective_url));
                nav_response.set_redirect_history(std::move(history));
            }

            return nav_response;
        }

        std::map<std::string, std::string> extract_cookies()
        {
            std::map<std::string, std::string> cookies;
            curl_slist *cookie_list = nullptr;
            curl_easy_getinfo(curl_handle, CURLINFO_COOKIELIST, &cookie_list);

            curl_slist *each = cookie_list;
            while (each != nullptr)
            {
                // netscape cookie format: domain\tTAILMATCH\tpath\tsecure\texpiry\tname\tvalue
                std::string line(each->data);
                auto parts = sihd::util::str::split(line, "\t");
                if (parts.size() >= 7)
                    cookies[std::string(parts[5])] = std::string(parts[6]);
                each = each->next;
            }

            if (cookie_list != nullptr)
                curl_slist_free_all(cookie_list);

            return cookies;
        }

        // WebSocket implementation

        static int ws_lws_callback(struct lws *wsi,
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
                        std::lock_guard lock(impl->ws_mutex);
                        impl->ws_connected = true;
                        impl->ws_handshake_done = true;
                    }
                    impl->ws_cv.notify_all();

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
                    std::lock_guard lock(impl->ws_mutex);
                    if (!impl->ws_send_queue.empty())
                    {
                        auto & msg = impl->ws_send_queue.front();
                        size_t data_len = msg.data.size();

                        std::vector<uint8_t> buf(LWS_PRE + data_len);
                        std::memcpy(buf.data() + LWS_PRE, msg.data.data(), data_len);

                        enum lws_write_protocol proto = msg.binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT;
                        int written = lws_write(wsi, buf.data() + LWS_PRE, data_len, proto);

                        impl->ws_send_queue.pop();

                        if (written < 0)
                        {
                            SIHD_LOG(error, "Navigator: WebSocket write failed");
                            return -1;
                        }

                        // if more messages pending, request another writable callback
                        if (!impl->ws_send_queue.empty())
                            lws_callback_on_writable(wsi);
                    }
                    break;
                }
                case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                {
                    const char *error_msg = (in != nullptr) ? static_cast<const char *>(in) : "unknown error";
                    SIHD_LOG(error, "Navigator: WebSocket connection error: {}", error_msg);
                    {
                        std::lock_guard lock(impl->ws_mutex);
                        impl->ws_connected = false;
                        impl->ws_handshake_done = true;
                    }
                    impl->ws_cv.notify_all();
                    break;
                }
                case LWS_CALLBACK_CLIENT_CLOSED:
                {
                    SIHD_LOG(debug, "Navigator: WebSocket connection closed");
                    {
                        std::lock_guard lock(impl->ws_mutex);
                        impl->ws_connected = false;
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
                        close_code = (uint16_t)(((uint8_t *)in)[0] << 8 | ((uint8_t *)in)[1]);
                        if (len > 2)
                            close_reason = std::string_view((const char *)in + 2, len - 2);
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

        bool ws_do_connect(std::string_view url, std::string_view protocol)
        {
            // parse url: ws://host:port/path or wss://host:port/path
            std::string url_str(url);
            bool use_ssl = false;
            std::string host;
            int port = 80;
            std::string path = "/";

            std::string_view remainder = url_str;
            if (remainder.starts_with("wss://"))
            {
                use_ssl = true;
                port = 443;
                remainder = remainder.substr(6);
            }
            else if (remainder.starts_with("ws://"))
            {
                remainder = remainder.substr(5);
            }

            // find path separator
            auto path_pos = remainder.find('/');
            std::string_view host_port;
            if (path_pos != std::string_view::npos)
            {
                host_port = remainder.substr(0, path_pos);
                path = std::string(remainder.substr(path_pos));
            }
            else
            {
                host_port = remainder;
            }

            // find port separator
            auto colon_pos = host_port.find(':');
            if (colon_pos != std::string_view::npos)
            {
                host = std::string(host_port.substr(0, colon_pos));
                port = std::stoi(std::string(host_port.substr(colon_pos + 1)));
            }
            else
            {
                host = std::string(host_port);
            }

            // build cookie header from curl cookie jar
            std::string cookie_header;
            auto cookies = this->extract_cookies();
            if (!cookies.empty())
            {
                for (const auto & [name, value] : cookies)
                {
                    if (!cookie_header.empty())
                        cookie_header += "; ";
                    cookie_header += fmt::format("{}={}", name, value);
                }
            }

            // setup lws protocol
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
            if (use_ssl)
                ctx_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

            ws_context = lws_create_context(&ctx_info);
            if (ws_context == nullptr)
            {
                SIHD_LOG(error, "Navigator: failed to create lws context for WebSocket client");
                return false;
            }

            struct lws_client_connect_info connect_info;
            memset(&connect_info, 0, sizeof(connect_info));
            connect_info.context = ws_context;
            connect_info.address = host.c_str();
            connect_info.port = port;
            connect_info.path = path.c_str();
            connect_info.host = host.c_str();
            connect_info.origin = host.c_str();
            connect_info.protocol = protocol_str.c_str();
            connect_info.userdata = this;
            connect_info.ietf_version_or_minus_one = -1;

            if (use_ssl)
            {
                int ssl_flags = LCCSCF_USE_SSL;
                if (!ssl_verify_peer)
                    ssl_flags |= LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
                connect_info.ssl_connection = ssl_flags;
            }

            ws_wsi = lws_client_connect_via_info(&connect_info);
            if (ws_wsi == nullptr)
            {
                SIHD_LOG(error, "Navigator: failed to initiate WebSocket connection");
                lws_context_destroy(ws_context);
                ws_context = nullptr;
                return false;
            }

            // start service loop thread
            ws_should_stop = false;
            ws_handshake_done = false;
            ws_connected = false;

            ws_thread = std::thread([this] {
                while (!ws_should_stop)
                {
                    lws_service(ws_context, 50);
                    {
                        std::lock_guard lock(ws_mutex);
                        if (!ws_connected && ws_handshake_done)
                            break;
                    }
                }
            });

            // wait for handshake to complete with timeout
            {
                std::unique_lock lock(ws_mutex);
                ws_cv.wait_for(lock, std::chrono::seconds(timeout_s), [this] { return ws_handshake_done; });
            }

            return ws_connected;
        }

        void ws_disconnect()
        {
            {
                std::lock_guard lock(ws_mutex);
                ws_should_stop = true;
            }

            if (ws_thread.joinable())
                ws_thread.join();

            if (ws_context != nullptr)
            {
                lws_context_destroy(ws_context);
                ws_context = nullptr;
            }
            ws_wsi = nullptr;
            ws_connected = false;
            ws_handshake_done = false;
        }

        bool ws_queue_message(const void *data, size_t len, bool binary)
        {
            std::lock_guard lock(ws_mutex);
            if (!ws_connected || ws_wsi == nullptr)
                return false;

            WsMessage msg;
            msg.data.assign(static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + len);
            msg.binary = binary;
            ws_send_queue.push(std::move(msg));

            lws_callback_on_writable(ws_wsi);
            lws_cancel_service(ws_context);

            return true;
        }
};

// Navigator public API

Navigator::Navigator(): _impl(std::make_unique<Impl>(this)) {}

Navigator::~Navigator() = default;

// HTTP methods

std::optional<NavigatorResponse> Navigator::get(std::string_view url)
{
    _impl->reset_handle_for_request();
    curl_easy_setopt(_impl->curl_handle, CURLOPT_HTTPGET, 1L);
    return _impl->perform_request(std::string(url));
}

std::optional<NavigatorResponse> Navigator::post(std::string_view url, sihd::util::ArrCharView data)
{
    _impl->reset_handle_for_request();
    curl_easy_setopt(_impl->curl_handle, CURLOPT_POST, 1L);
    if (!data.empty())
    {
        curl_easy_setopt(_impl->curl_handle, CURLOPT_POSTFIELDSIZE, (long)data.size());
        curl_easy_setopt(_impl->curl_handle, CURLOPT_COPYPOSTFIELDS, data.data());
    }
    return _impl->perform_request(std::string(url));
}

std::optional<NavigatorResponse> Navigator::put(std::string_view url, sihd::util::ArrCharView data)
{
    _impl->reset_handle_for_request();
    curl_easy_setopt(_impl->curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    if (!data.empty())
    {
        curl_easy_setopt(_impl->curl_handle, CURLOPT_POSTFIELDSIZE, (long)data.size());
        curl_easy_setopt(_impl->curl_handle, CURLOPT_COPYPOSTFIELDS, data.data());
    }
    return _impl->perform_request(std::string(url));
}

std::optional<NavigatorResponse> Navigator::del(std::string_view url)
{
    _impl->reset_handle_for_request();
    curl_easy_setopt(_impl->curl_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
    return _impl->perform_request(std::string(url));
}

std::optional<NavigatorResponse> Navigator::options(std::string_view url)
{
    _impl->reset_handle_for_request();
    curl_easy_setopt(_impl->curl_handle, CURLOPT_CUSTOMREQUEST, "OPTIONS");
    curl_easy_setopt(_impl->curl_handle, CURLOPT_NOBODY, 1L);
    return _impl->perform_request(std::string(url));
}

// Configuration

void Navigator::set_verbose(bool verbose)
{
    _impl->verbose = verbose;
}

void Navigator::set_follow_redirects(bool follow)
{
    _impl->follow_redirects = follow;
}

void Navigator::set_max_redirects(long max)
{
    _impl->max_redirects = max;
}

void Navigator::set_timeout(long seconds)
{
    _impl->timeout_s = seconds;
}

void Navigator::set_ssl_verify(bool verify_peer, bool verify_host)
{
    _impl->ssl_verify_peer = verify_peer;
    _impl->ssl_verify_host = verify_host;
}

void Navigator::set_user_agent(std::string_view agent)
{
    _impl->user_agent = std::string(agent);
}

// Authentication

void Navigator::set_basic_auth(std::string_view user, std::string_view password)
{
    _impl->username = std::string(user);
    _impl->password = std::string(password);
    _impl->token.clear();
}

void Navigator::set_token_auth(std::string_view token)
{
    _impl->token = std::string(token);
    _impl->username.clear();
    _impl->password.clear();
}

void Navigator::clear_auth()
{
    _impl->username.clear();
    _impl->password.clear();
    _impl->token.clear();
}

// Headers

void Navigator::set_header(std::string_view name, std::string_view value)
{
    _impl->headers[std::string(name)] = std::string(value);
}

void Navigator::remove_header(std::string_view name)
{
    _impl->headers.erase(std::string(name));
}

void Navigator::clear_headers()
{
    _impl->headers.clear();
}

// Cookies

std::map<std::string, std::string> Navigator::cookies() const
{
    return _impl->extract_cookies();
}

void Navigator::set_cookie(std::string_view name, std::string_view value, std::string_view domain)
{
    std::string domain_str = domain.empty() ? "." : std::string(domain);
    // netscape cookie format for CURLOPT_COOKIELIST
    std::string cookie_line = fmt::format("{}\tTRUE\t/\tFALSE\t0\t{}\t{}", domain_str, name, value);
    curl_easy_setopt(_impl->curl_handle, CURLOPT_COOKIELIST, cookie_line.c_str());
}

void Navigator::clear_cookies()
{
    curl_easy_setopt(_impl->curl_handle, CURLOPT_COOKIELIST, "ALL");
}

// WebSocket

bool Navigator::ws_connect(std::string_view url, std::string_view protocol)
{
    return _impl->ws_do_connect(url, protocol);
}

bool Navigator::ws_send(std::string_view text)
{
    return _impl->ws_queue_message(text.data(), text.size(), false);
}

bool Navigator::ws_send_binary(sihd::util::ArrByteView data)
{
    return _impl->ws_queue_message(data.buf(), data.byte_size(), true);
}

void Navigator::ws_close()
{
    _impl->ws_disconnect();
}

bool Navigator::ws_is_connected() const
{
    std::lock_guard lock(_impl->ws_mutex);
    return _impl->ws_connected;
}

} // namespace sihd::http
