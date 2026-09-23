#include <sihd/http/Navigator.hpp>
#include <sihd/http/navigator/CsrfExtractor.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Url.hpp>

#include "navigator/NavigatorImpl.hpp"

namespace sihd::http
{

using sihd::util::Url;

SIHD_NEW_LOGGER("sihd::http");

// ---- Navigator public API ----

Navigator::Navigator(): _impl(std::make_unique<Impl>(this)) {}

Navigator::~Navigator() = default;

// HTTP methods

std::optional<NavigatorResponse> Navigator::get(std::string_view url)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url)});
}

std::optional<NavigatorResponse> Navigator::post(std::string_view url, sihd::util::ArrCharView data)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .type = HttpRequest::Post, .body = data});
}

std::optional<NavigatorResponse> Navigator::patch(std::string_view url, sihd::util::ArrCharView data)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .type = HttpRequest::Patch, .body = data});
}

std::optional<NavigatorResponse> Navigator::head(std::string_view url)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .type = HttpRequest::Head});
}

std::optional<NavigatorResponse> Navigator::put(std::string_view url, sihd::util::ArrCharView data)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .type = HttpRequest::Put, .body = data});
}

std::optional<NavigatorResponse> Navigator::del(std::string_view url)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .type = HttpRequest::Delete});
}

std::optional<NavigatorResponse> Navigator::options(std::string_view url)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .type = HttpRequest::Options});
}

std::optional<NavigatorResponse> Navigator::post_multipart(std::string_view url, const Multipart & multipart)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .type = HttpRequest::Post, .multipart = multipart});
}

std::optional<NavigatorResponse> Navigator::download(std::string_view url, std::string_view path)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .download_path = std::string(path)});
}

std::optional<NavigatorResponse> Navigator::put_file(std::string_view url, std::string_view path)
{
    _impl->reset_for_request();
    return _impl->perform({.url = std::string(url), .type = HttpRequest::Put, .upload_path = std::string(path)});
}

long Navigator::new_connection_count() const
{
    return _impl->client.new_connection_count();
}

std::string Navigator::last_error() const
{
    return _impl->last_error;
}

// Configuration

void Navigator::set_verbose(bool verbose)
{
    _impl->options.verbose = verbose;
}

void Navigator::set_follow_redirects(bool follow)
{
    _impl->redirects.follow = follow;
}

void Navigator::set_max_redirects(long max)
{
    _impl->redirects.max = max;
}

void Navigator::set_redirect_policy(RedirectPolicy policy)
{
    _impl->redirects.policy = policy;
}

void Navigator::set_timeout(sihd::util::Duration timeout)
{
    _impl->options.timeout = timeout;
}

void Navigator::set_connect_timeout(sihd::util::Duration timeout)
{
    _impl->options.connect_timeout = timeout;
}

void Navigator::set_accept_encoding(bool enable)
{
    _impl->options.accept_encoding = enable;
}

void Navigator::set_http2(bool enable)
{
    _impl->options.http2 = enable;
}

void Navigator::set_ssl_verify(bool verify_peer, bool verify_host)
{
    _impl->options.ssl_verify_peer = verify_peer;
    _impl->options.ssl_verify_host = verify_host;
}

void Navigator::set_user_agent(std::string_view agent)
{
    _impl->options.user_agent = std::string(agent);
}

void Navigator::set_max_response_size(size_t bytes)
{
    _impl->options.max_response_size = bytes;
}

void Navigator::set_ssrf_guard(bool enable)
{
    _impl->ssrf_guard = enable;
}

// Authentication

void Navigator::set_basic_auth(std::string_view user, std::string_view password)
{
    _impl->auth.username = std::string(user);
    _impl->auth.password = std::string(password);
    _impl->auth.token.clear();
    _impl->auth.digest = false;
}

void Navigator::set_digest_auth(std::string_view user, std::string_view password)
{
    _impl->auth.username = std::string(user);
    _impl->auth.password = std::string(password);
    _impl->auth.token.clear();
    _impl->auth.digest = true;
}

void Navigator::set_token_auth(std::string_view token)
{
    _impl->auth.token = std::string(token);
    _impl->auth.username.clear();
    _impl->auth.password.clear();
    _impl->auth.digest = false;
}

void Navigator::clear_auth()
{
    _impl->auth.username.clear();
    _impl->auth.password.clear();
    _impl->auth.token.clear();
    _impl->auth.digest = false;
}

bool Navigator::form_login(const FormLoginParams & params)
{
    auto page = get(params.login_url);
    if (!page.has_value())
        return false;

    std::string body = page->content().cpp_str();

    std::string csrf_field = params.csrf_field;
    std::string csrf_value;

    std::string_view hdr_token = page->http_header().find("x-csrf-token:");
    if (!hdr_token.empty())
        csrf_value = std::string(hdr_token);

    if (csrf_value.empty())
    {
        CsrfResult cr = csrf_extract_from_html(body,
                                               csrf_field.empty() ? std::nullopt : std::make_optional(csrf_field));
        csrf_field = cr.field_name;
        csrf_value = cr.value;
    }

    std::string post_body = fmt::format("{}={}&{}={}",
                                        Url::str_encode(params.username_field),
                                        Url::str_encode(params.username),
                                        Url::str_encode(params.password_field),
                                        Url::str_encode(params.password));
    if (!csrf_field.empty() && !csrf_value.empty())
        post_body += fmt::format("&{}={}", Url::str_encode(csrf_field), Url::str_encode(csrf_value));

    set_header("Content-Type", "application/x-www-form-urlencoded");
    auto resp = post(params.login_url, post_body);
    remove_header("Content-Type");

    if (!resp.has_value())
        return false;

    return resp->status() < 400;
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
    std::string cookie_line = fmt::format("{}\tTRUE\t/\tFALSE\t0\t{}\t{}", domain_str, name, value);
    _impl->client.add_cookie(cookie_line);
}

void Navigator::clear_cookies()
{
    _impl->client.clear_cookies();
}

void Navigator::save_cookies(std::string_view path)
{
    _impl->client.save_cookies(path);
}

void Navigator::load_cookies(std::string_view path)
{
    _impl->client.load_cookies(path);
}

// Proxy

void Navigator::set_proxy(std::string_view url, ProxyType type)
{
    _impl->proxy.url = std::string(url);
    _impl->proxy.type = type;
    _impl->proxy.disabled = false;
}

void Navigator::set_proxy_auth(std::string_view user, std::string_view password)
{
    _impl->proxy.user = std::string(user);
    _impl->proxy.pass = std::string(password);
}

void Navigator::clear_proxy()
{
    _impl->proxy.url.clear();
    _impl->proxy.user.clear();
    _impl->proxy.pass.clear();
    _impl->proxy.pool.clear();
    _impl->proxy.disabled = true;
}

void Navigator::add_proxy(std::string_view url, ProxyType type)
{
    _impl->proxy.pool.push_back({std::string(url), type});
    _impl->proxy.disabled = false;
}

void Navigator::set_proxy_rotation(ProxyRotation rotation)
{
    _impl->proxy.rotation = rotation;
}

// Retry

void Navigator::set_retry(int max_attempts, long initial_backoff_ms)
{
    _impl->rate.retry_max = max_attempts;
    _impl->rate.retry_initial_backoff_ms = initial_backoff_ms;
}

// Rate limiting

void Navigator::set_domain_delay_ms(long ms)
{
    _impl->rate.domain_delay_ms = ms;
}

// User-agent rotation

void Navigator::add_user_agent(std::string_view ua)
{
    _impl->rate.user_agent_pool.emplace_back(ua);
}

void Navigator::set_user_agent_rotation(bool enable)
{
    _impl->rate.ua_rotation = enable;
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
    auto lock = _impl->ws.waitable.guard();
    return _impl->ws.connected;
}

} // namespace sihd::http
