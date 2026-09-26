#include <sihd/http/HttpStatus.hpp>
#include <sihd/net/ip.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Url.hpp>
#include <sihd/util/str.hpp>

#include "NavigatorImpl.hpp"

namespace sihd::http
{

using sihd::util::Url;

SIHD_LOGGER;

namespace
{

long parse_retry_after(std::string_view value)
{
    if (value.empty())
        return 0;
    if (const auto n = sihd::util::str::convert_from_string<long>(value))
        return *n;
    if (auto ts = sihd::util::Timestamp::from_str(std::string(value), "%a, %d %b %Y %H:%M:%S GMT"))
    {
        long delta = static_cast<long>((static_cast<int64_t>(*ts) - static_cast<int64_t>(sihd::util::Timestamp::now()))
                                       / 1'000'000'000LL);
        return delta > 0 ? delta : 0;
    }
    return 0;
}

// curl accepts a URL without scheme, Url does not: parse it as http like curl does
Url parse_navigator_url(std::string_view url)
{
    Url parsed(url);
    if (parsed.scheme.empty() && parsed.host.empty())
        return Url(fmt::format("http://{}", url));
    return parsed;
}

std::string extract_meta_refresh_url(const std::string & body)
{
    static const std::string meta_pattern = R"(<meta[^>]+http-equiv\s*=\s*["']?refresh["']?[^>]*>)";
    auto metas = sihd::util::str::regex_search(body, meta_pattern);
    if (metas.empty())
        return {};
    auto url_kv = sihd::util::str::regex_search(metas[0], R"(url\s*=\s*[^"'\s>;]+)");
    if (url_kv.empty())
        return {};
    auto [_, url] = sihd::util::str::split_pair_view(url_kv[0], "=");
    return std::string(sihd::util::str::trim(url));
}

} // namespace

Navigator::Impl::Impl(Navigator *nav): owner(nav)
{
    client.enable_cookie_engine();
}

Navigator::Impl::~Impl()
{
    ws_disconnect();
}

std::vector<std::string> Navigator::Impl::extract_raw_cookies()
{
    return client.cookie_list();
}

void Navigator::Impl::reset_for_request()
{
    auto saved = extract_raw_cookies();

    client.reset();
    client.enable_cookie_engine();

    for (const auto & line : saved)
        client.add_cookie(line);
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
    {
        result.error = request_result.error().message;
        last_error = result.error;
    }

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

    auto reg_domain = [](std::string_view h) -> std::string {
        auto parts = sihd::util::str::split(h, ".");
        if (parts.size() <= 2)
            return std::string(h);
        return std::string(parts[parts.size() - 2]) + "." + std::string(parts[parts.size() - 1]);
    };
    return reg_domain(orig.host) == reg_domain(tgt.host);
}

std::optional<Navigator::Impl::SingleResponse> Navigator::Impl::try_perform(const std::string & url,
                                                                            HttpRequest::RequestType type,
                                                                            const RequestOptions & options,
                                                                            const Navigation & navigation)
{
    long backoff = rate.retry_initial_backoff_ms;
    for (int attempt = 0; attempt <= rate.retry_max; ++attempt)
    {
        auto sr = perform_single(url, type, options, navigation);
        if (sr.overflow)
        {
            SIHD_LOG(warning, "Navigator: response truncated (exceeded size limit) for {}", url);
            return sr;
        }
        if (!sr.ok)
        {
            SIHD_LOG(error, "Navigator: request failed: {}", sr.error);
            if (attempt < rate.retry_max)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                backoff *= 2;
                continue;
            }
            return std::nullopt;
        }
        if (HttpStatus::is_rate_limit(sr.http_status) && attempt < rate.retry_max)
        {
            std::string_view ra = sr.response.http_header().find("retry-after:");
            long wait_s = parse_retry_after(ra);
            SIHD_LOG(notice,
                     "Navigator: status {} → retrying in {}ms (attempt {}/{})",
                     sr.http_status,
                     wait_s > 0 ? wait_s * 1000 : backoff,
                     attempt + 1,
                     rate.retry_max);
            if (wait_s > 0)
                std::this_thread::sleep_for(std::chrono::seconds(wait_s));
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
            backoff *= 2;
            continue;
        }
        return sr;
    }
    return std::nullopt;
}

std::optional<NavigatorResponse> Navigator::Impl::perform(const Navigation & navigation)
{
    last_error.clear();

    if (ssrf_guard && sihd::net::ip::is_private_host(parse_navigator_url(navigation.url).host))
    {
        SIHD_LOG(error, "Navigator: SSRF guard blocked request to private host: {}", navigation.url);
        last_error = "SSRF guard blocked a request to a private host";
        return std::nullopt;
    }

    HttpRequest::RequestType type = navigation.type;
    RequestOptions options = make_options();
    // a downgraded or refreshed navigation drops its payload
    Navigation attempt = navigation;

    std::string current_url = navigation.url;

    if (owner->on_before_request)
    {
        RequestInfo info;
        info.url = current_url;
        info.method = HttpRequest::type_str(type);
        info.headers = options.headers;
        if (!owner->on_before_request(info))
        {
            last_error = "request cancelled by the on_before_request interceptor";
            return std::nullopt;
        }
        current_url = info.url;
        options.headers = std::move(info.headers);
        if (HttpRequest::RequestType requested = HttpRequest::type_from_str(info.method);
            requested != HttpRequest::None)
            type = requested;
    }

    enforce_domain_delay(current_url);
    std::vector<std::string> history = {current_url};
    bool method_downgraded = false;
    bool can_downgrade = (type != HttpRequest::Get && type != HttpRequest::Head && type != HttpRequest::Options);
    int redirect_count = 0;

    // a redirected navigation continues as a GET without its payload
    auto downgrade_to_get = [&attempt, &type]() {
        type = HttpRequest::Get;
        attempt.body = {};
        attempt.multipart.clear();
        attempt.upload_path.clear();
    };

    while (true)
    {
        options.multipart = attempt.multipart;

        auto sr_opt = try_perform(current_url, type, options, attempt);
        if (!sr_opt)
            return std::nullopt;

        auto & sr = *sr_opt;

        if (sr.http_status == 401 && owner->on_auth_required)
        {
            if (owner->on_auth_required())
            {
                RequestOptions retry_options = options;
                this->apply_auth(retry_options);

                auto retry_sr = perform_single(current_url, type, retry_options, attempt);
                if (retry_sr.ok)
                    sr = std::move(retry_sr);
            }
        }

        bool do_redirect = false;
        std::string next_url;

        if (redirects.follow && HttpStatus::is_redirect(sr.http_status) && redirect_count < redirects.max)
        {
            std::string_view loc_hdr = sr.response.http_header().find("location:");
            if (!loc_hdr.empty())
                next_url = std::string(loc_hdr);
            else if (!sr.redirect_location.empty())
                next_url = sr.redirect_location;

            while (!next_url.empty() && (next_url.back() == '\r' || next_url.back() == '\n' || next_url.back() == ' '))
                next_url.pop_back();

            if (!next_url.empty() && check_redirect_policy(navigation.url, next_url))
            {
                do_redirect = true;

                if (can_downgrade && HttpStatus::is_post_downgrade(sr.http_status))
                {
                    method_downgraded = true;
                    can_downgrade = false;
                    downgrade_to_get();
                }

                if (ssrf_guard && sihd::net::ip::is_private_host(parse_navigator_url(next_url).host))
                {
                    SIHD_LOG(error, "Navigator: SSRF guard blocked redirect to {}", next_url);
                    do_redirect = false;
                }
                else
                {
                    enforce_domain_delay(next_url);
                    history.push_back(next_url);
                    current_url = next_url;
                    ++redirect_count;
                }
            }
        }

        if (!do_redirect)
        {
            if (redirects.follow && redirect_count < redirects.max && !sr.response.content().empty())
            {
                std::string meta_url = extract_meta_refresh_url(sr.response.content().cpp_str());
                if (!meta_url.empty() && check_redirect_policy(navigation.url, meta_url))
                {
                    if (!ssrf_guard || !sihd::net::ip::is_private_host(parse_navigator_url(meta_url).host))
                    {
                        enforce_domain_delay(meta_url);
                        history.push_back(meta_url);
                        current_url = meta_url;
                        ++redirect_count;
                        downgrade_to_get();
                        continue;
                    }
                }
            }

            NavigatorResponse nav_response(std::move(sr.response));
            nav_response.set_final_url(std::string(current_url));
            nav_response.set_cookies(extract_cookies());

            if (history.size() > 1)
                nav_response.set_redirect_history(std::move(history));

            nav_response.set_method_downgraded(method_downgraded);

            if (owner->on_after_response)
                owner->on_after_response(nav_response);

            return nav_response;
        }
    }
}

std::map<std::string, std::string> Navigator::Impl::extract_cookies()
{
    std::map<std::string, std::string> cookies;
    for (const auto & line : extract_raw_cookies())
    {
        auto parts = sihd::util::str::split(line, "\t");
        if (parts.size() >= 7)
            cookies[std::string(parts[5])] = std::string(parts[6]);
    }
    return cookies;
}

} // namespace sihd::http
