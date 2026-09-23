#ifndef __SIHD_HTTP_NAVIGATOR_IMPL_HPP__
#define __SIHD_HTTP_NAVIGATOR_IMPL_HPP__

#include <atomic>
#include <chrono>
#include <map>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/http/RequestOptions.hpp>
#include <sihd/http/navigator/NavigatorResponse.hpp>
#include <sihd/util/Url.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>

#include "../Client.hpp"
#include "../lws.hpp"

namespace sihd::http
{

struct Navigator::Impl
{
        Client client;
        Navigator *owner = nullptr;
        std::string last_error;

        // every request starts from these and overrides what it needs
        RequestOptions options;

        // curl never follows redirects for us, the navigator does it to apply its policy
        struct RedirectConfig
        {
                bool follow = true;
                long max = 10;
                RedirectPolicy policy = RedirectPolicy::All;
        } redirects;
        bool ssrf_guard = false;

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
                sihd::util::Waitable waitable;
                bool connected = false;
                bool handshake_done = false;
                std::atomic<bool> stop_requested {false};
                std::queue<Message> send_queue;
        } ws;

        struct Navigation
        {
                std::string url;
                HttpRequest::RequestType type = HttpRequest::Get;
                sihd::util::ArrCharView body = {};
                Multipart multipart = {};
                std::string upload_path = {};
                std::string download_path = {};
        };

        struct SingleResponse
        {
                HttpResponse response;
                bool overflow = false;
                bool ok = true;
                std::string error;
                int http_status = 0;
                std::string redirect_location;
        };

        Impl(Navigator *nav);
        ~Impl();

        RequestOptions make_options();
        void apply_auth(RequestOptions & options);
        void reset_for_request();
        std::optional<std::pair<std::string, ProxyType>> select_proxy();
        void enforce_domain_delay(const std::string & url);
        SingleResponse perform_single(const std::string & url,
                                      HttpRequest::RequestType type,
                                      const RequestOptions & options,
                                      const Navigation & navigation);
        bool check_redirect_policy(const std::string & original_url, const std::string & target_url) const;
        std::optional<SingleResponse> try_perform(const std::string & url,
                                                  HttpRequest::RequestType type,
                                                  const RequestOptions & options,
                                                  const Navigation & navigation);
        std::optional<NavigatorResponse> perform(const Navigation & navigation);
        std::map<std::string, std::string> extract_cookies();
        std::vector<std::string> extract_raw_cookies();

        // WebSocket methods (NavigatorWebSocket.cpp)
        static int ws_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
        std::string ws_proxy_address();
        bool ws_do_connect(std::string_view url, std::string_view protocol);
        void ws_disconnect();
        bool ws_queue_message(const void *data, size_t len, bool binary);
};

} // namespace sihd::http

#endif
