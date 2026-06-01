#ifndef __SIHD_HTTP_NAVIGATOR_IMPL_HPP__
#define __SIHD_HTTP_NAVIGATOR_IMPL_HPP__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/util/Url.hpp>
#include <sihd/http/navigator/NavigatorResponse.hpp>
#include <sihd/util/Worker.hpp>

#include "../curl/utils.hpp"
#include "../lws.hpp"

namespace sihd::http
{

struct ContentWriteCtx
{
        std::string *content;
        size_t max_size;
        bool overflow;
};

struct Navigator::Impl
{
        CURL *curl_handle = nullptr;
        Navigator *owner = nullptr;

        struct HttpConfig
        {
                bool verbose = false;
                bool follow_redirects = true;
                long max_redirects = 10;
                RedirectPolicy redirect_policy = RedirectPolicy::All;
                long timeout_s = 30;
                long connect_timeout_s = 10;
                bool accept_encoding = true;
                bool http2 = false;
                bool ssl_verify_peer = true;
                bool ssl_verify_host = true;
                size_t max_response_size = 0;
                bool ssrf_guard = false;
                std::string user_agent;
        } http;

        struct AuthConfig
        {
                std::string username;
                std::string password;
                std::string token;
                bool digest = false;
        } auth;

        std::map<std::string, std::string> headers;

        struct ProxyConfig
        {
                struct Entry
                {
                        std::string url;
                        ProxyType type;
                };
                std::string url;
                ProxyType type = ProxyType::Http;
                std::string user;
                std::string pass;
                std::vector<Entry> pool;
                ProxyRotation rotation = ProxyRotation::None;
                size_t pool_idx = 0;
                bool disabled = false;
        } proxy;

        struct RateLimitConfig
        {
                int retry_max = 0;
                long retry_initial_backoff_ms = 500;
                long domain_delay_ms = 0;
                std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_request_time;
                std::vector<std::string> user_agent_pool;
                bool ua_rotation = false;
                std::mt19937 rng {std::random_device {}()};
        } rate;

        struct WsState
        {
                struct Message
                {
                        std::vector<uint8_t> data;
                        bool binary;
                };
                struct lws_context *context = nullptr;
                struct lws *wsi = nullptr;
                sihd::util::Worker worker;
                std::mutex mutex;
                std::condition_variable cv;
                bool connected = false;
                bool handshake_done = false;
                std::atomic<bool> stop_requested {false};
                std::queue<Message> send_queue;
        } ws;

        struct SingleResponse
        {
                HttpResponse response;
                std::string content;
                bool overflow = false;
                CURLcode code = CURLE_OK;
                int http_status = 0;
                std::string redirect_location;
        };

        struct RequestOptions
        {
                FILE *download_fp;
        };

        void set_request_body(sihd::util::ArrCharView data);

        // HTTP methods (NavigatorHttp.cpp)
        Impl(Navigator *nav);
        ~Impl();

        std::vector<std::string> extract_raw_cookies();
        void reset_handle_for_request();
        void apply_proxy();
        curl_slist *build_headers();
        void configure_handle(const std::string & url,
                              HttpResponse & response,
                              ContentWriteCtx & write_ctx,
                              FILE *download_fp = nullptr);
        void enforce_domain_delay(const std::string & url);
        SingleResponse perform_single(const std::string & url, FILE *download_fp = nullptr);
        bool check_redirect_policy(const std::string & original_url, const std::string & target_url) const;
        std::optional<SingleResponse> try_perform(const std::string & url, FILE *download_fp = nullptr);
        std::optional<NavigatorResponse> perform_request(const std::string & start_url,
                                                         std::string_view method,
                                                         RequestOptions opts = RequestOptions{});
        std::optional<NavigatorResponse> perform_multipart(const std::string & url,
                                                           const std::vector<MultipartField> & fields);
        std::optional<NavigatorResponse> perform_download(const std::string & url, const std::string & path);
        std::map<std::string, std::string> extract_cookies();

        // WebSocket methods (NavigatorWebSocket.cpp)
        static int ws_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
        bool ws_do_connect(std::string_view url, std::string_view protocol);
        void ws_disconnect();
        bool ws_queue_message(const void *data, size_t len, bool binary);
};

} // namespace sihd::http

#endif
