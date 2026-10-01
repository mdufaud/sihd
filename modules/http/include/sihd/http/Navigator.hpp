#ifndef __SIHD_HTTP_NAVIGATOR_HPP__
#define __SIHD_HTTP_NAVIGATOR_HPP__

#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include <sihd/http/Multipart.hpp>
#include <sihd/http/RequestOptions.hpp>
#include <sihd/http/navigator/NavigatorResponse.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::http
{

enum class RedirectPolicy
{
    All,        // follow all redirects (default)
    SameHost,   // only follow redirects to the exact same host
    SameDomain, // only follow redirects within the same registered domain
    None,       // do not follow any redirect
};

enum class ProxyRotation
{
    None,
    RoundRobin,
    Random,
};

struct RequestInfo
{
        std::string url;
        std::string method;
        std::map<std::string, std::string> headers;
};

struct FormLoginParams
{
        std::string login_url;
        std::string username_field = "username";
        std::string password_field = "password";
        std::string username;
        std::string password;
        // optional: if empty, CsrfExtractor will detect the field name automatically
        std::string csrf_field;
};

class Navigator
{
    public:
        Navigator();
        ~Navigator();

        Navigator(const Navigator &) = delete;
        Navigator & operator=(const Navigator &) = delete;
        Navigator(Navigator &&) = delete;
        Navigator & operator=(Navigator &&) = delete;

        // HTTP methods
        std::expected<NavigatorResponse, sihd::util::Error> get(std::string_view url);
        std::expected<NavigatorResponse, sihd::util::Error> post(std::string_view url,
                                                                 sihd::util::ArrCharView data = {});
        std::expected<NavigatorResponse, sihd::util::Error> put(std::string_view url, sihd::util::ArrCharView data);
        std::expected<NavigatorResponse, sihd::util::Error> patch(std::string_view url, sihd::util::ArrCharView data);
        std::expected<NavigatorResponse, sihd::util::Error> del(std::string_view url);
        std::expected<NavigatorResponse, sihd::util::Error> head(std::string_view url);
        std::expected<NavigatorResponse, sihd::util::Error> options(std::string_view url);
        std::expected<NavigatorResponse, sihd::util::Error> post_multipart(std::string_view url,
                                                                           const Multipart & multipart);
        std::expected<NavigatorResponse, sihd::util::Error> download(std::string_view url, std::string_view path);
        // streams the file through the request instead of loading it in memory
        std::expected<NavigatorResponse, sihd::util::Error> put_file(std::string_view url, std::string_view path);

        // connections opened by the last request; 0 means an existing one was reused
        long new_connection_count() const;

        // Configuration
        void set_verbose(bool verbose);
        void set_follow_redirects(bool follow);
        void set_max_redirects(long max);
        void set_redirect_policy(RedirectPolicy policy);
        void set_timeout(sihd::util::Duration timeout);
        void set_connect_timeout(sihd::util::Duration timeout);
        void set_accept_encoding(bool enable);
        void set_http2(bool enable);
        void set_ssl_verify(bool verify_peer, bool verify_host);
        void set_user_agent(std::string_view agent);
        void set_max_response_size(size_t bytes);
        void set_ssrf_guard(bool enable);

        // Authentication (persistent across requests)
        void set_basic_auth(std::string_view user, std::string_view password);
        void set_digest_auth(std::string_view user, std::string_view password);
        void set_token_auth(std::string_view token);
        void clear_auth();

        // Form-based login (auto CSRF extraction)
        std::expected<void, sihd::util::Error> form_login(const FormLoginParams & params);

        // Persistent headers
        void set_header(std::string_view name, std::string_view value);
        void remove_header(std::string_view name);
        void clear_headers();

        // Cookies (in-memory cookie jar)
        std::map<std::string, std::string> cookies() const;
        void set_cookie(std::string_view name, std::string_view value, std::string_view domain = "");
        void clear_cookies();
        void save_cookies(std::string_view path);
        void load_cookies(std::string_view path);

        // Proxy
        void set_proxy(std::string_view url, ProxyType type = ProxyType::Http);
        void set_proxy_auth(std::string_view user, std::string_view password);
        void clear_proxy();
        void add_proxy(std::string_view url, ProxyType type = ProxyType::Http);
        void set_proxy_rotation(ProxyRotation rotation);

        // Retry
        void set_retry(int max_attempts, long initial_backoff_ms = 500);

        // Per-domain rate limiting
        void set_domain_delay_ms(long ms);

        // User-agent rotation
        void add_user_agent(std::string_view ua);
        void set_user_agent_rotation(bool enable);

        // WebSocket
        bool ws_connect(std::string_view url, std::string_view protocol = "");
        bool ws_send(std::string_view text);
        bool ws_send_binary(sihd::util::ArrByteView data);
        void ws_close();
        bool ws_is_connected() const;

        // Interceptors (return false from on_before_request to cancel)
        std::function<bool(RequestInfo &)> on_before_request;
        std::function<void(const NavigatorResponse &)> on_after_response;

        // Callbacks
        std::function<void(std::string_view protocol)> on_ws_open;
        std::function<void(std::string_view message)> on_ws_text;
        std::function<void(sihd::util::ArrByteView data)> on_ws_binary;
        std::function<void()> on_ws_close;
        std::function<void(uint16_t code, std::string_view reason)> on_ws_peer_close;
        // called when a 401 or redirect-to-login is detected; return true to retry
        std::function<bool()> on_auth_required;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::http

#endif
