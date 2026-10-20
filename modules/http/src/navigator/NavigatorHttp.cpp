#include <algorithm>

#include <sihd/curl/utils.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/scheme.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Url.hpp>
#include <sihd/util/str.hpp>

#include "HtmlScanner.hpp"
#include "NavigatorImpl.hpp"

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::http
{

namespace
{

struct UrlScheme
{
        std::string name;
        bool has_authority = false;
};

struct Origin
{
        std::string scheme;
        std::string host;
        int port = 0;

        bool operator==(const Origin &) const = default;
};

long parse_retry_after(std::string_view value)
{
    if (value.empty())
        return 0;
    if (const auto seconds = str::convert_from_string<long>(value))
        return *seconds;
    if (auto ts = Timestamp::from_str(std::string(value), "%a, %d %b %Y %H:%M:%S GMT"))
    {
        long delta = static_cast<long>((static_cast<int64_t>(*ts) - static_cast<int64_t>(Timestamp::now()))
                                       / 1'000'000'000LL);
        return delta > 0 ? delta : 0;
    }
    return 0;
}

// curl takes "host:port/path" as http, Url takes "host" for a scheme
UrlScheme navigator_scheme(std::string_view url)
{
    const Url parsed(url);
    if (parsed.scheme.empty())
        return {"http", false};
    if (parsed.has_authority)
        return {str::to_lower_copy(parsed.scheme), true};
    const std::string_view port = std::string_view(parsed.path).substr(0, parsed.path.find('/'));
    if (!port.empty() && std::ranges::all_of(port, [](char digit) { return str::is_digit(digit); }))
        return {"http", false};
    return {str::to_lower_copy(parsed.scheme), false};
}

Url parse_navigator_url(std::string_view url)
{
    if (navigator_scheme(url).has_authority)
        return Url(url);
    return Url(fmt::format("http://{}", url));
}

bool is_http_url(std::string_view url)
{
    const std::string scheme = navigator_scheme(url).name;
    return scheme == "http" || scheme == "https";
}

Origin origin_of(std::string_view url)
{
    const Url parsed = parse_navigator_url(url);
    const std::string scheme = navigator_scheme(url).name;
    return {scheme, str::to_lower_copy(parsed.host), parsed.port > 0 ? parsed.port : scheme_port(scheme)};
}

bool carries_payload(HttpRequest::RequestType type)
{
    return type != HttpRequest::Get && type != HttpRequest::Head && type != HttpRequest::Options;
}

void strip_credentials(RequestOptions & options, const std::vector<std::string> & kept_headers)
{
    options.username.clear();
    options.password.clear();
    options.token.clear();
    options.digest = false;
    std::erase_if(options.headers, [&kept_headers](const auto & header) {
        return std::ranges::none_of(kept_headers,
                                    [&header](const std::string & kept) { return str::iequals(header.first, kept); });
    });
}

constexpr size_t meta_refresh_scan_limit = 64 * 1024;

bool maybe_html(std::optional<std::string_view> content_type)
{
    if (!content_type)
        return true;
    return str::to_lower_copy(*content_type).find("html") != std::string::npos;
}

std::string refresh_target(std::string_view content)
{
    const std::string lowered = str::to_lower_copy(content);
    size_t key = lowered.find("url");
    while (key != std::string::npos && key > 0
           && std::string_view(" \t;,").find(lowered[key - 1]) == std::string_view::npos)
        key = lowered.find("url", key + 1);
    if (key == std::string::npos)
        return {};

    std::string_view rest = str::ltrim(content.substr(key + 3));
    if (!rest.starts_with('='))
        return {};
    rest = str::ltrim(rest.substr(1));
    if (rest.starts_with('"') || rest.starts_with('\''))
        return std::string(rest.substr(1, rest.find(rest[0], 1) - 1));
    return std::string(rest.substr(0, rest.find_first_of(" \t\r\n;")));
}

std::string extract_meta_refresh_url(std::string_view body)
{
    body = body.substr(0, meta_refresh_scan_limit);
    std::string url;
    htmlscan::scan_tags(body, [&url](const htmlscan::Tag & tag) {
        if (!url.empty() || tag.name != "meta")
            return;
        const auto equiv = tag.attribute("http-equiv");
        const auto content = tag.attribute("content");
        if (equiv.has_value() && content.has_value() && str::iequals(*equiv, "refresh"))
            url = refresh_target(*content);
    });
    return url;
}

} // namespace

Navigator::Impl::Impl(Navigator *nav): owner(nav)
{
    // a defunct handle fails this too: every request reports it
    (void)client.enable_cookie_engine();
}

Navigator::Impl::~Impl()
{
    ws_disconnect();
}

std::vector<std::string> Navigator::Impl::extract_raw_cookies()
{
    return client.cookie_list();
}

Result<void> Navigator::Impl::reset_for_request()
{
    auto saved = extract_raw_cookies();

    client.reset();
    auto engine = client.enable_cookie_engine();
    SIHD_UNEXPECTED_RETURN(engine);

    for (const auto & line : saved)
    {
        auto restored = client.add_cookie(line);
        SIHD_UNEXPECTED_RETURN(restored);
    }
    return {};
}

std::optional<std::pair<std::string, ProxyType>> Navigator::Impl::select_proxy()
{
    if (proxy.rotation != ProxyRotation::None && !proxy.pool.empty())
    {
        size_t idx = 0;
        if (proxy.rotation == ProxyRotation::RoundRobin)
        {
            idx = proxy.pool_idx % proxy.pool.size();
            proxy.pool_idx++;
        }
        else
        {
            std::uniform_int_distribution<size_t> dist(0, proxy.pool.size() - 1);
            idx = dist(rate.rng);
        }
        return std::make_pair(proxy.pool[idx].url, proxy.pool[idx].type);
    }
    if (!proxy.url.empty())
        return std::make_pair(proxy.url, proxy.type);
    return std::nullopt;
}

void Navigator::Impl::apply_auth(RequestOptions & options)
{
    options.username = auth.username;
    options.password = auth.password;
    options.token = auth.token;
    options.digest = auth.digest;
}

RequestOptions Navigator::Impl::make_options()
{
    RequestOptions options = this->options;

    // never let curl follow redirects — we do it manually
    options.follow_location = false;
    options.headers = headers;

    if (rate.ua_rotation && !rate.user_agent_pool.empty())
    {
        std::uniform_int_distribution<size_t> dist(0, rate.user_agent_pool.size() - 1);
        options.user_agent = rate.user_agent_pool[dist(rate.rng)];
    }

    this->apply_auth(options);

    if (proxy.disabled)
    {
        options.proxy = "";
    }
    else if (auto selected = select_proxy(); selected.has_value())
    {
        options.proxy = selected->first;
        options.proxy_type = selected->second;
        options.proxy_username = proxy.user;
        options.proxy_password = proxy.pass;
    }

    return options;
}

void Navigator::Impl::enforce_domain_delay(const std::string & url)
{
    if (rate.domain_delay_ms <= 0)
        return;
    std::string host = parse_navigator_url(url).host;
    auto it = rate.last_request_time.find(host);
    if (it != rate.last_request_time.end())
    {
        auto elapsed = std::chrono::steady_clock::now() - it->second;
        auto needed = std::chrono::milliseconds(rate.domain_delay_ms);
        if (elapsed < needed)
            std::this_thread::sleep_for(needed - elapsed);
    }
    rate.last_request_time[host] = std::chrono::steady_clock::now();
}

Navigator::Impl::SingleResponse Navigator::Impl::perform_single(const std::string & url,
                                                                HttpRequest::RequestType type,
                                                                const RequestOptions & options,
                                                                const Navigation & navigation)
{
    SingleResponse result;

    Client::Result request_result;
    if (!navigation.download_path.empty())
        request_result = client.receive_file(url, navigation.download_path, options, result.response);
    else if (!navigation.upload_path.empty())
        request_result = client.send_file(url, navigation.upload_path, type, options, result.response);
    else
        request_result = client.send(url, type, navigation.body, options, result.response);
    result.ok = request_result.has_value();

    result.overflow = client.overflow();

    if (!result.ok && !result.overflow)
        result.error = std::move(request_result).error();

    if (result.ok || result.overflow)
    {
        result.http_status = (int)result.response.status();
        result.redirect_location = client.redirect_url();
    }

    return result;
}

bool Navigator::Impl::check_redirect_policy(const std::string & original_url, const std::string & target_url) const
{
    if (redirects.policy == RedirectPolicy::All)
        return true;
    if (redirects.policy == RedirectPolicy::None)
        return false;

    auto orig = parse_navigator_url(original_url);
    auto tgt = parse_navigator_url(target_url);

    if (redirects.policy == RedirectPolicy::SameHost)
        return orig.host == tgt.host;

    // the last two labels of an address are no domain
    if (sihd::net::ip::is_valid_ip(orig.host) || sihd::net::ip::is_valid_ip(tgt.host))
        return orig.host == tgt.host;

    auto reg_domain = [](std::string_view h) -> std::string {
        auto parts = str::split(h, ".");
        if (parts.size() <= 2)
            return std::string(h);
        return std::string(parts[parts.size() - 2]) + "." + std::string(parts[parts.size() - 1]);
    };
    return reg_domain(orig.host) == reg_domain(tgt.host);
}

Result<std::vector<std::string>> Navigator::Impl::check_target(const std::string & url) const
{
    if (!is_http_url(url))
        return std::unexpected(Error(invalid_argument, "Navigator: only http and https URLs are allowed: {}", url));
    if (!ssrf_guard)
        return std::vector<std::string> {};
    auto addresses = sihd::net::ip::public_addresses(parse_navigator_url(url).host);
    if (!addresses)
        return std::unexpected(Error(permission_denied, "SSRF guard blocked a request to a private host: {}", url));
    return addresses;
}

Result<Navigator::Impl::SingleResponse> Navigator::Impl::try_perform(const std::string & url,
                                                                     HttpRequest::RequestType type,
                                                                     const RequestOptions & options,
                                                                     const Navigation & navigation)
{
    const long max_wait_ms = rate.retry_max_wait_ms;
    long backoff = std::min(rate.retry_initial_backoff_ms, max_wait_ms);
    for (int attempt = 0; attempt <= rate.retry_max; ++attempt)
    {
        auto sr = perform_single(url, type, options, navigation);
        if (sr.overflow)
            return sr;
        if (!sr.ok)
        {
            if (sr.error.retryable() && attempt < rate.retry_max)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                backoff = std::min(backoff * 2, max_wait_ms);
                continue;
            }
            return std::unexpected(std::move(sr.error));
        }
        if (HttpStatus::is_rate_limit(sr.http_status) && attempt < rate.retry_max)
        {
            std::string_view ra = sr.response.http_header().find("retry-after:");
            const long wait_s = parse_retry_after(ra);
            const long wait_ms = wait_s > max_wait_ms / 1000 ? max_wait_ms : wait_s * 1000;
            const long sleep_ms = wait_ms > 0 ? wait_ms : backoff;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            backoff = std::min(backoff * 2, max_wait_ms);
            continue;
        }
        return sr;
    }
    return std::unexpected(Error(unknown, "retry loop exited without a response"));
}

struct Navigator::Impl::Walk
{
        Walk(const Navigation & navigation, RequestOptions request_options):
            type(navigation.type),
            options(std::move(request_options)),
            attempt(navigation),
            requested_url(navigation.url),
            current_url(navigation.url)
        {
        }

        void settle()
        {
            initial_origin = origin_of(current_url);
            origin = initial_origin;
            history = {current_url};
            can_downgrade = carries_payload(type);
        }

        void downgrade_to_get()
        {
            type = HttpRequest::Get;
            attempt.body = {};
            attempt.multipart.clear();
            attempt.upload_path.clear();
        }

        HttpRequest::RequestType type;
        RequestOptions options;
        // a downgraded or refreshed navigation drops its payload
        Navigation attempt;

        std::string requested_url;
        std::string current_url;
        std::vector<std::string> vetted;

        Origin initial_origin;
        Origin origin;
        std::vector<std::string> history;
        std::string redirect_refusal;

        int redirect_count = 0;
        bool method_downgraded = false;
        bool can_downgrade = false;
};

void Navigator::Impl::advance(Walk & walk, const std::string & url, std::vector<std::string> && vetted)
{
    if (const Origin target = origin_of(url); target != walk.origin)
    {
        strip_credentials(walk.options, cross_origin_headers);
        walk.origin = target;
    }
    enforce_domain_delay(url);
    walk.vetted = std::move(vetted);
    walk.history.push_back(url);
    walk.current_url = url;
    ++walk.redirect_count;
}

Result<void> Navigator::Impl::intercept(Walk & walk)
{
    if (!owner->on_before_request)
        return {};

    RequestInfo info;
    info.url = walk.current_url;
    info.method = HttpRequest::type_str(walk.type);
    info.headers = walk.options.headers;
    if (!owner->on_before_request(info))
        return std::unexpected(Error(interrupted, "request cancelled by the on_before_request interceptor"));

    if (info.url != walk.current_url)
    {
        auto vetted = this->check_target(info.url);
        SIHD_UNEXPECTED_RETURN(vetted);
        walk.vetted = std::move(*vetted);
        walk.current_url = info.url;
    }
    walk.options.headers = std::move(info.headers);
    if (HttpRequest::RequestType requested = HttpRequest::type_from_str(info.method); requested != HttpRequest::None)
        walk.type = requested;
    return {};
}

// the client would resolve the host again: the vetted addresses close the rebinding window
void Navigator::Impl::pin_target(Walk & walk)
{
    if (!ssrf_guard)
        return;
    const Origin target = origin_of(walk.current_url);
    walk.options.resolve.reset();
    if (!sihd::net::ip::is_valid_ip(target.host))
        walk.options.resolve = ResolvePin {target.host, target.port, walk.vetted};
}

Result<Navigator::Impl::SingleResponse> Navigator::Impl::fetch(Walk & walk)
{
    walk.options.multipart = walk.attempt.multipart;
    this->pin_target(walk);

    auto sr_res = this->try_perform(walk.current_url, walk.type, walk.options, walk.attempt);
    SIHD_UNEXPECTED_RETURN(sr_res);
    auto & sr = *sr_res;

    if (sr.overflow && !walk.attempt.download_path.empty())
        return std::unexpected(
            Error(overflow, "download exceeds the {}-byte response limit", walk.options.max_response_size));

    if (sr.http_status != 401 || !owner->on_auth_required || walk.origin != walk.initial_origin)
        return sr_res;
    if (!owner->on_auth_required())
        return sr_res;

    RequestOptions retry_options = walk.options;
    this->apply_auth(retry_options);
    auto retry_sr = this->perform_single(walk.current_url, walk.type, retry_options, walk.attempt);
    if (!retry_sr.ok && !retry_sr.overflow)
        return std::unexpected(std::move(retry_sr.error));
    sr = std::move(retry_sr);
    return sr_res;
}

std::string Navigator::Impl::location_target(const Walk & walk, const SingleResponse & sr) const
{
    if (!redirects.follow || !HttpStatus::is_redirect(sr.http_status) || walk.redirect_count >= redirects.max)
        return {};

    std::string_view header_location = sr.response.http_header().find("location:");
    std::string next_url = sr.redirect_location.empty() ? std::string(header_location) : sr.redirect_location;
    while (!next_url.empty() && (next_url.back() == '\r' || next_url.back() == '\n' || next_url.back() == ' '))
        next_url.pop_back();

    if (next_url.empty() || !this->check_redirect_policy(walk.requested_url, next_url))
        return {};
    return next_url;
}

bool Navigator::Impl::follow_location(Walk & walk, const SingleResponse & sr)
{
    const std::string next_url = this->location_target(walk, sr);
    if (next_url.empty())
        return false;

    if (walk.can_downgrade && HttpStatus::is_post_downgrade(sr.http_status))
    {
        walk.method_downgraded = true;
        walk.can_downgrade = false;
        walk.downgrade_to_get();
    }

    auto vetted = this->check_target(next_url);
    if (!vetted)
    {
        walk.redirect_refusal = vetted.error().message;
        return false;
    }
    if (origin_of(next_url) != walk.origin && carries_payload(walk.type))
    {
        walk.redirect_refusal = fmt::format("cross-origin redirect refused for a request with a payload: {}", next_url);
        return false;
    }

    this->advance(walk, next_url, std::move(*vetted));
    return true;
}

bool Navigator::Impl::follow_meta_refresh(Walk & walk, const SingleResponse & sr)
{
    if (!redirects.follow || walk.redirect_count >= redirects.max)
        return false;
    if (!maybe_html(sr.response.http_header().content_type()) || sr.response.content().empty())
        return false;

    std::string meta_url = extract_meta_refresh_url(sr.response.content().cpp_str_view());
    if (!meta_url.empty())
        meta_url = sihd::curl::resolve_url(walk.current_url, meta_url).value_or(std::string());
    if (meta_url.empty() || !this->check_redirect_policy(walk.requested_url, meta_url))
        return false;

    auto vetted = this->check_target(meta_url);
    if (!vetted)
    {
        walk.redirect_refusal = vetted.error().message;
        return false;
    }

    this->advance(walk, meta_url, std::move(*vetted));
    walk.downgrade_to_get();
    return true;
}

NavigatorResponse Navigator::Impl::make_response(Walk & walk, SingleResponse & sr)
{
    NavigatorResponse nav_response(std::move(sr.response));
    nav_response.set_final_url(std::string(walk.current_url));
    nav_response.set_cookies(this->extract_cookies());

    if (walk.history.size() > 1)
        nav_response.set_redirect_history(std::move(walk.history));

    nav_response.set_method_downgraded(walk.method_downgraded);
    nav_response.set_truncated(sr.overflow);
    if (!walk.redirect_refusal.empty())
        nav_response.set_redirect_refusal(std::move(walk.redirect_refusal));

    if (owner->on_after_response)
        owner->on_after_response(nav_response);
    return nav_response;
}

Result<NavigatorResponse> Navigator::Impl::perform(const Navigation & navigation)
{
    auto vetted = this->check_target(navigation.url);
    SIHD_UNEXPECTED_RETURN(vetted);

    Walk walk(navigation, make_options());
    walk.vetted = std::move(*vetted);

    auto intercepted = this->intercept(walk);
    SIHD_UNEXPECTED_RETURN(intercepted);

    walk.settle();
    enforce_domain_delay(walk.current_url);

    while (true)
    {
        auto fetched = this->fetch(walk);
        SIHD_UNEXPECTED_RETURN(fetched);

        if (this->follow_location(walk, *fetched) || this->follow_meta_refresh(walk, *fetched))
            continue;
        return this->make_response(walk, *fetched);
    }
}

std::map<std::string, std::string> Navigator::Impl::extract_cookies()
{
    std::map<std::string, std::string> cookies;
    for (const auto & line : extract_raw_cookies())
    {
        auto parts = str::split(line, "\t");
        if (parts.size() >= 7)
            cookies[std::string(parts[5])] = std::string(parts[6]);
    }
    return cookies;
}

} // namespace sihd::http
