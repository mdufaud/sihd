#ifndef __SIHD_HTTP_NAVIGATOR_IMPL_HPP__
#define __SIHD_HTTP_NAVIGATOR_IMPL_HPP__

#include <atomic>
#include <chrono>
#include <limits>
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
#include <sihd/util/Result.hpp>
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

        // every request starts from these and overrides what it needs
        RequestOptions options;

        // curl never follows redirects for us, the navigator does it to apply its policy
        struct RedirectConfig
        {
                bool follow = true;
                long max = 10;
                RedirectPolicy policy = RedirectPolicy::All;
        } redirects;
        bool ssrf_guard = true;

        struct AuthConfig
        {
                std::string username;
                std::string password;
                std::string token;
                bool digest = false;
        } auth;

        std::map<std::string, std::string> headers;
        std::vector<std::string> cross_origin_headers;

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
                long retry_max_wait_ms = std::numeric_limits<long>::max();
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
                struct LwsContextFree
                {
                        void operator()(struct lws_context *context) const;
                };

                std::unique_ptr<struct lws_context, LwsContextFree> context;
                struct lws *wsi = nullptr;
                sihd::util::Worker worker;
                sihd::util::Waitable waitable;
                bool connected = false;
                bool handshake_done = false;
                std::string connect_error;
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
                sihd::util::Error error;
                int http_status = 0;
                std::string redirect_location;
        };

        Impl(Navigator *nav);
        ~Impl();

        RequestOptions make_options();
        void apply_auth(RequestOptions & options);
        sihd::util::Result<void> reset_for_request();
        std::optional<std::pair<std::string, ProxyType>> select_proxy();
        void enforce_domain_delay(const std::string & url);
        SingleResponse perform_single(const std::string & url,
                                      HttpRequest::RequestType type,
                                      const RequestOptions & options,
                                      const Navigation & navigation);
        bool check_redirect_policy(const std::string & original_url, const std::string & target_url) const;
        sihd::util::Result<std::vector<std::string>> check_target(const std::string & url) const;
        sihd::util::Result<SingleResponse> try_perform(const std::string & url,
                                                       HttpRequest::RequestType type,
                                                       const RequestOptions & options,
                                                       const Navigation & navigation);
        sihd::util::Result<NavigatorResponse> perform(const Navigation & navigation);

        struct Walk;
        sihd::util::Result<void> intercept(Walk & walk);
        void pin_target(Walk & walk);
        sihd::util::Result<SingleResponse> fetch(Walk & walk);
        std::string location_target(const Walk & walk, const SingleResponse & sr) const;
        bool follow_location(Walk & walk, const SingleResponse & sr);
        bool follow_meta_refresh(Walk & walk, const SingleResponse & sr);
        void advance(Walk & walk, const std::string & url, std::vector<std::string> && vetted);
        NavigatorResponse make_response(Walk & walk, SingleResponse & sr);
        std::map<std::string, std::string> extract_cookies();
        std::vector<std::string> extract_raw_cookies();

        // WebSocket methods (NavigatorWebSocket.cpp)
        static int ws_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
        sihd::util::Result<std::string> ws_proxy_address();
        sihd::util::Result<void> ws_do_connect(std::string_view url, sihd::util::ZStringView protocol);
        void ws_disconnect();
        bool ws_queue_message(const void *data, size_t len, bool binary);
};

} // namespace sihd::http

#endif
