#include <stdexcept>

#include <sihd/util/build.hpp>

#if defined(__SIHD_WINDOWS__)
# include <io.h> // _fileno _chsize_s
#else
# include <unistd.h> // ftruncate
#endif

#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/navigator/CsrfExtractor.hpp>
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

size_t content_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *ctx = static_cast<ContentWriteCtx *>(userdata);
    size_t incoming = size * nmemb;
    if (ctx->max_size > 0 && ctx->content->size() + incoming > ctx->max_size)
    {
        ctx->overflow = true;
        return 0;
    }
    ctx->content->append(ptr, incoming);
    return incoming;
}

size_t header_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *header = static_cast<HttpHeader *>(userdata);
    std::string_view header_str(ptr, size * nmemb);
    header->add_header_from_str(header_str);
    return size * nmemb;
}

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
    curl_init_once();
    curl_handle = curl_easy_init();
    if (curl_handle == nullptr)
        throw std::runtime_error("could not initialize curl");
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
}

Navigator::Impl::~Impl()
{
    ws_disconnect();
    if (curl_handle != nullptr)
        curl_easy_cleanup(curl_handle);
}

void Navigator::Impl::set_request_body(sihd::util::ArrCharView data)
{
    if (!data.empty())
    {
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)data.size());
        curl_easy_setopt(curl_handle, CURLOPT_COPYPOSTFIELDS, data.data());
    }
}

std::vector<std::string> Navigator::Impl::extract_raw_cookies()
{
    std::vector<std::string> lines;
    curl_slist *cookie_list = nullptr;
    curl_easy_getinfo(curl_handle, CURLINFO_COOKIELIST, &cookie_list);
    for (auto *each = cookie_list; each != nullptr; each = each->next)
        lines.emplace_back(each->data);
    if (cookie_list != nullptr)
        curl_slist_free_all(cookie_list);
    return lines;
}

void Navigator::Impl::reset_handle_for_request()
{
    auto saved = extract_raw_cookies();

    curl_easy_reset(curl_handle);
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");

    for (const auto & line : saved)
        curl_easy_setopt(curl_handle, CURLOPT_COOKIELIST, line.c_str());

    if (rate.ua_rotation && !rate.user_agent_pool.empty())
    {
        std::uniform_int_distribution<size_t> dist(0, rate.user_agent_pool.size() - 1);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, rate.user_agent_pool[dist(rate.rng)].c_str());
    }

    apply_proxy();
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

void Navigator::Impl::apply_proxy()
{
    if (proxy.disabled)
    {
        curl_easy_setopt(curl_handle, CURLOPT_PROXY, "");
        return;
    }

    auto selected = select_proxy();
    if (!selected.has_value())
        return;

    const auto & [pu, pt] = *selected;

    curl_easy_setopt(curl_handle, CURLOPT_PROXY, pu.c_str());
    long curl_proxy_type = CURLPROXY_HTTP;
    if (pt == ProxyType::Socks4)
        curl_proxy_type = CURLPROXY_SOCKS4;
    else if (pt == ProxyType::Socks5)
        curl_proxy_type = CURLPROXY_SOCKS5;
    curl_easy_setopt(curl_handle, CURLOPT_PROXYTYPE, curl_proxy_type);
    if (!proxy.user.empty() && !proxy.pass.empty())
    {
        curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERNAME, proxy.user.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_PROXYPASSWORD, proxy.pass.c_str());
    }
}

curl_slist *Navigator::Impl::build_headers()
{
    curl_slist *slist = nullptr;

    if (!auth.token.empty())
    {
        std::string bearer = fmt::format("Authorization: Bearer {}", auth.token);
        slist = curl_slist_append(slist, bearer.c_str());
    }

    for (const auto & [name, value] : headers)
    {
        std::string h = fmt::format("{}: {}", name, value);
        slist = curl_slist_append(slist, h.c_str());
    }

    return slist;
}

void Navigator::Impl::configure_handle(const std::string & url,
                                       HttpResponse & response,
                                       ContentWriteCtx & write_ctx,
                                       FILE *download_fp)
{
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, (long)http.verbose);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());

    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &response.http_header());

    if (download_fp != nullptr)
    {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, nullptr);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, download_fp);
    }
    else
    {
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, content_write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &write_ctx);
    }

    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, http.timeout_s);
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, http.connect_timeout_s);

    // never let curl follow redirects — we do it manually
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 0L);

    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, (long)http.ssl_verify_peer);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, http.ssl_verify_host ? 2L : 0L);

    if (http.accept_encoding)
        curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

    if (http.http2)
        curl_easy_setopt(curl_handle, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);

    if (!http.user_agent.empty() && !rate.ua_rotation)
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, http.user_agent.c_str());

    if (!auth.username.empty() && !auth.password.empty())
    {
        curl_easy_setopt(curl_handle, CURLOPT_USERNAME, auth.username.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, auth.password.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, auth.digest ? (long)CURLAUTH_DIGEST : (long)CURLAUTH_BASIC);
    }
}

void Navigator::Impl::enforce_domain_delay(const std::string & url)
{
    if (rate.domain_delay_ms <= 0)
        return;
    std::string host = Url(url).host;
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

Navigator::Impl::SingleResponse Navigator::Impl::perform_single(const std::string & url, FILE *download_fp)
{
    SingleResponse result;
    ContentWriteCtx write_ctx {&result.content, http.max_response_size, false};

    configure_handle(url, result.response, write_ctx, download_fp);

    curl_slist *slist = build_headers();
    if (slist != nullptr)
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);

    result.code = curl_easy_perform(curl_handle);

    if (slist != nullptr)
        curl_slist_free_all(slist);

    result.overflow = write_ctx.overflow;

    if (result.code == CURLE_OK || result.code == CURLE_WRITE_ERROR)
    {
        long status = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status);
        result.http_status = static_cast<int>(status);

        char *loc = nullptr;
        curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_URL, &loc);
        if (loc != nullptr)
            result.redirect_location = loc;

        char *ct = nullptr;
        curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
        if (ct != nullptr)
            result.response.set_content_type(ct);
    }

    return result;
}

bool Navigator::Impl::check_redirect_policy(const std::string & original_url, const std::string & target_url) const
{
    if (http.redirect_policy == RedirectPolicy::All)
        return true;
    if (http.redirect_policy == RedirectPolicy::None)
        return false;

    auto orig = Url(original_url);
    auto tgt = Url(target_url);

    if (http.redirect_policy == RedirectPolicy::SameHost)
        return orig.host == tgt.host;

    auto reg_domain = [](std::string_view h) -> std::string {
        auto parts = sihd::util::str::split(h, ".");
        if (parts.size() <= 2)
            return std::string(h);
        return std::string(parts[parts.size() - 2]) + "." + std::string(parts[parts.size() - 1]);
    };
    return reg_domain(orig.host) == reg_domain(tgt.host);
}

std::optional<Navigator::Impl::SingleResponse> Navigator::Impl::try_perform(const std::string & url, FILE *download_fp)
{
    long backoff = rate.retry_initial_backoff_ms;
    for (int attempt = 0; attempt <= rate.retry_max; ++attempt)
    {
        auto sr = perform_single(url, download_fp);
        if (sr.overflow)
        {
            SIHD_LOG(warning, "Navigator: response truncated (exceeded size limit) for {}", url);
            return sr;
        }
        if (sr.code != CURLE_OK)
        {
            SIHD_LOG(error, "Navigator: request failed: {}", curl_easy_strerror(sr.code));
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

std::optional<NavigatorResponse>
    Navigator::Impl::perform_request(const std::string & start_url, std::string_view method, RequestOptions opts)
{
    if (http.ssrf_guard && sihd::net::ip::is_private_host(Url(start_url).host))
    {
        SIHD_LOG(error, "Navigator: SSRF guard blocked request to private host: {}", start_url);
        return std::nullopt;
    }

    std::string current_url = start_url;

    if (owner->on_before_request)
    {
        RequestInfo info;
        info.url = current_url;
        info.method = std::string(method);
        info.headers = headers;
        if (!owner->on_before_request(info))
            return std::nullopt;
        current_url = info.url;
    }

    enforce_domain_delay(current_url);
    std::vector<std::string> history = {current_url};
    bool method_downgraded = false;
    bool can_downgrade = (method != "GET" && method != "HEAD" && method != "OPTIONS");
    int redirects = 0;

    while (true)
    {
        if (opts.download_fp != nullptr)
        {
            fflush(opts.download_fp);
#if defined(__SIHD_WINDOWS__)
            _chsize_s(_fileno(opts.download_fp), 0);
#else
            if (ftruncate(fileno(opts.download_fp), 0) != 0)
                SIHD_LOG(warning, "Navigator: failed to truncate download file before retry");
#endif
            rewind(opts.download_fp);
        }

        auto sr_opt = try_perform(current_url, opts.download_fp);
        if (!sr_opt)
            return std::nullopt;

        auto & sr = *sr_opt;

        if (sr.http_status == 401 && owner->on_auth_required)
        {
            if (owner->on_auth_required())
            {
                auto retry_sr = perform_single(current_url, opts.download_fp);
                if (retry_sr.code == CURLE_OK)
                    sr = std::move(retry_sr);
            }
        }

        bool do_redirect = false;
        std::string next_url;

        if (http.follow_redirects && HttpStatus::is_redirect(sr.http_status) && redirects < http.max_redirects)
        {
            std::string_view loc_hdr = sr.response.http_header().find("location:");
            if (!loc_hdr.empty())
                next_url = std::string(loc_hdr);
            else if (!sr.redirect_location.empty())
                next_url = sr.redirect_location;

            while (!next_url.empty() && (next_url.back() == '\r' || next_url.back() == '\n' || next_url.back() == ' '))
                next_url.pop_back();

            if (!next_url.empty() && check_redirect_policy(start_url, next_url))
            {
                do_redirect = true;

                if (can_downgrade && HttpStatus::is_post_downgrade(sr.http_status))
                {
                    method_downgraded = true;
                    can_downgrade = false;
                    curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);
                }

                if (http.ssrf_guard && sihd::net::ip::is_private_host(Url(next_url).host))
                {
                    SIHD_LOG(error, "Navigator: SSRF guard blocked redirect to {}", next_url);
                    do_redirect = false;
                }
                else
                {
                    enforce_domain_delay(next_url);
                    history.push_back(next_url);
                    current_url = next_url;
                    redirects++;
                }
            }
        }

        if (!do_redirect)
        {
            if (http.follow_redirects && redirects < http.max_redirects && !sr.content.empty())
            {
                std::string meta_url = extract_meta_refresh_url(sr.content);
                if (!meta_url.empty() && check_redirect_policy(start_url, meta_url))
                {
                    if (!http.ssrf_guard || !sihd::net::ip::is_private_host(Url(meta_url).host))
                    {
                        enforce_domain_delay(meta_url);
                        history.push_back(meta_url);
                        current_url = meta_url;
                        redirects++;
                        curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);
                        continue;
                    }
                }
            }

            sr.response.set_status(sr.http_status);
            sr.response.set_content(sr.content);

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

std::optional<NavigatorResponse> Navigator::Impl::perform_download(const std::string & url, const std::string & path)
{
    FILE *fp = fopen(path.c_str(), "wb");
    if (fp == nullptr)
    {
        SIHD_LOG(error, "Navigator: cannot open file for download: {}", path);
        return std::nullopt;
    }

    auto result = perform_request(url, "GET", RequestOptions {.download_fp = fp});
    fclose(fp);

    return result;
}

std::optional<NavigatorResponse> Navigator::Impl::perform_multipart(const std::string & url,
                                                                    const std::vector<MultipartField> & fields)
{
    curl_mime *mime = curl_mime_init(curl_handle);

    for (const auto & field : fields)
    {
        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_name(part, field.name.c_str());

        if (field.is_file)
        {
            curl_mime_filedata(part, field.value.c_str());
            if (!field.filename.empty())
                curl_mime_filename(part, field.filename.c_str());
        }
        else
        {
            curl_mime_data(part, field.value.c_str(), field.value.size());
        }

        if (!field.content_type.empty())
            curl_mime_type(part, field.content_type.c_str());
    }

    curl_easy_setopt(curl_handle, CURLOPT_MIMEPOST, mime);

    auto result = perform_request(url, "POST");

    curl_mime_free(mime);

    return result;
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
