#include <sihd/http/Navigator.hpp>
#include <sihd/http/navigator/CsrfExtractor.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Url.hpp>

#include "navigator/NavigatorImpl.hpp"

using namespace sihd::util;

using enum sihd::util::ErrorCode;

namespace sihd::http
{

SIHD_NEW_LOGGER("sihd::http");

Navigator::Navigator(): _impl(std::make_unique<Impl>(this)) {}

Navigator::~Navigator() = default;

Result<NavigatorResponse> Navigator::get(const std::string & url)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url});
}

Result<NavigatorResponse> Navigator::post(const std::string & url, ArrCharView data)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .type = HttpRequest::Post, .body = data});
}

Result<NavigatorResponse> Navigator::patch(const std::string & url, ArrCharView data)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .type = HttpRequest::Patch, .body = data});
}

Result<NavigatorResponse> Navigator::head(const std::string & url)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .type = HttpRequest::Head});
}

Result<NavigatorResponse> Navigator::put(const std::string & url, ArrCharView data)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .type = HttpRequest::Put, .body = data});
}

Result<NavigatorResponse> Navigator::del(const std::string & url)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .type = HttpRequest::Delete});
}

Result<NavigatorResponse> Navigator::options(const std::string & url)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .type = HttpRequest::Options});
}

Result<NavigatorResponse> Navigator::post_multipart(const std::string & url, const Multipart & multipart)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .type = HttpRequest::Post, .multipart = multipart});
}

Result<NavigatorResponse> Navigator::download(const std::string & url, const std::string & path)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .download_path = path});
}

Result<NavigatorResponse> Navigator::put_file(const std::string & url, const std::string & path)
{
    auto reset = _impl->reset_for_request();
    SIHD_UNEXPECTED_RETURN(reset);
    return _impl->perform({.url = url, .type = HttpRequest::Put, .upload_path = path});
}

long Navigator::new_connection_count() const
{
    return _impl->client.new_connection_count();
}

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

void Navigator::set_timeout(Duration timeout)
{
    _impl->options.timeout = timeout;
}

void Navigator::set_connect_timeout(Duration timeout)
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

void Navigator::set_user_agent(const std::string & agent)
{
    _impl->options.user_agent = agent;
}

void Navigator::set_max_response_size(size_t bytes)
{
    _impl->options.max_response_size = bytes;
}

void Navigator::set_ssrf_guard(bool enable)
{
    _impl->ssrf_guard = enable;
}

void Navigator::set_basic_auth(const std::string & user, const std::string & password)
{
    _impl->auth.username = user;
    _impl->auth.password = password;
    _impl->auth.token.clear();
    _impl->auth.digest = false;
}

void Navigator::set_digest_auth(const std::string & user, const std::string & password)
{
    _impl->auth.username = user;
    _impl->auth.password = password;
    _impl->auth.token.clear();
    _impl->auth.digest = true;
}

void Navigator::set_token_auth(const std::string & token)
{
    _impl->auth.token = token;
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

Result<void> Navigator::form_login(const FormLoginParams & params)
{
    auto page = get(params.login_url);
    SIHD_UNEXPECTED_RETURN(page);

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

    SIHD_UNEXPECTED_RETURN(resp);

    if (resp->status() >= 400)
        return std::unexpected(Error(permission_denied, "login failed with status {}", resp->status()));
    return {};
}

void Navigator::set_header(const std::string & name, const std::string & value)
{
    _impl->headers[name] = value;
}

void Navigator::remove_header(const std::string & name)
{
    _impl->headers.erase(name);
}

void Navigator::clear_headers()
{
    _impl->headers.clear();
}

void Navigator::set_cross_origin_headers(std::vector<std::string> names)
{
    _impl->cross_origin_headers = std::move(names);
}

std::map<std::string, std::string> Navigator::cookies() const
{
    return _impl->extract_cookies();
}

Result<void> Navigator::set_cookie(std::string_view name, std::string_view value, std::string_view domain)
{
    const std::string_view domain_str = domain.empty() ? std::string_view(".") : domain;
    std::string cookie_line = fmt::format("{}\tTRUE\t/\tFALSE\t0\t{}\t{}", domain_str, name, value);
    return _impl->client.add_cookie(cookie_line);
}

Result<void> Navigator::clear_cookies()
{
    return _impl->client.clear_cookies();
}

Result<void> Navigator::save_cookies(const std::string & path)
{
    return _impl->client.save_cookies(path);
}

Result<void> Navigator::load_cookies(const std::string & path)
{
    return _impl->client.load_cookies(path);
}

void Navigator::set_proxy(const std::string & url, ProxyType type)
{
    _impl->proxy.url = url;
    _impl->proxy.type = type;
    _impl->proxy.disabled = false;
}

void Navigator::set_proxy_auth(const std::string & user, const std::string & password)
{
    _impl->proxy.user = user;
    _impl->proxy.pass = password;
}

void Navigator::clear_proxy()
{
    _impl->proxy.url.clear();
    _impl->proxy.user.clear();
    _impl->proxy.pass.clear();
    _impl->proxy.pool.clear();
    _impl->proxy.disabled = true;
}

void Navigator::add_proxy(const std::string & url, ProxyType type)
{
    _impl->proxy.pool.push_back({url, type});
    _impl->proxy.disabled = false;
}

void Navigator::set_proxy_rotation(ProxyRotation rotation)
{
    _impl->proxy.rotation = rotation;
}

void Navigator::set_retry(int max_attempts, long initial_backoff_ms, long max_wait_ms)
{
    _impl->rate.retry_max = max_attempts;
    _impl->rate.retry_initial_backoff_ms = initial_backoff_ms;
    _impl->rate.retry_max_wait_ms = max_wait_ms > 0 ? max_wait_ms : std::numeric_limits<long>::max();
}

void Navigator::set_domain_delay_ms(long ms)
{
    _impl->rate.domain_delay_ms = ms;
}

void Navigator::add_user_agent(const std::string & ua)
{
    _impl->rate.user_agent_pool.emplace_back(ua);
}

void Navigator::set_user_agent_rotation(bool enable)
{
    _impl->rate.ua_rotation = enable;
}

Result<void> Navigator::ws_connect(std::string_view url, ZStringView protocol)
{
    return _impl->ws_do_connect(url, protocol);
}

bool Navigator::ws_send(std::string_view text)
{
    return _impl->ws_queue_message(text.data(), text.size(), false);
}

bool Navigator::ws_send_binary(ArrByteView data)
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
