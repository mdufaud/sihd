#include <fmt/format.h>

#include <sihd/http/navigator/LinkExtractor.hpp>
#include <sihd/util/str.hpp>

namespace sihd::http
{

namespace
{

std::string resolve_url(std::string_view href, std::string_view base_url)
{
    if (href.empty())
        return {};
    if (href.starts_with("http://") || href.starts_with("https://") || href.starts_with("//"))
        return std::string(href);
    if (base_url.empty())
        return std::string(href);

    std::string base(base_url);
    while (!base.empty() && base.back() == '/')
        base.pop_back();

    if (href.starts_with("/"))
    {
        auto scheme_end = base.find("://");
        if (scheme_end != std::string::npos)
        {
            auto host_end = base.find('/', scheme_end + 3);
            if (host_end != std::string::npos)
                return base.substr(0, host_end) + std::string(href);
        }
        return base + std::string(href);
    }

    auto last_slash = base.rfind('/');
    auto scheme_end = base.find("://");
    if (last_slash != std::string::npos && (scheme_end == std::string::npos || last_slash > scheme_end + 2))
        return base.substr(0, last_slash + 1) + std::string(href);

    return base + "/" + std::string(href);
}

std::string extract_attr(const std::string & tag, std::string_view attr_name)
{
    std::string pattern = fmt::format(R"({}\s*=\s*["'][^"']*["'])", attr_name);
    auto matches = sihd::util::str::regex_search(tag, pattern);
    if (matches.empty())
        return {};

    auto [_, raw] = sihd::util::str::split_pair_view(matches[0], "=");
    raw = sihd::util::str::trim(raw);
    if (raw.size() >= 2 && (raw.front() == '"' || raw.front() == '\''))
        return std::string(raw.substr(1, raw.size() - 2));
    return std::string(raw);
}

void extract_tag(std::vector<ExtractedLink> & out,
                 const std::string & html,
                 std::string_view tag_name,
                 std::string_view url_attr,
                 std::string_view base_url)
{
    std::string pattern;
    if (tag_name == "a")
        pattern = fmt::format(R"(<a\s[^>]*{}\s*=\s*["'][^"']*["'][^>]*>[^<]*</a>)", url_attr);
    else
        pattern = fmt::format(R"(<{}\s[^>]*{}\s*=\s*["'][^"']*["'][^>]*>)", tag_name, url_attr);

    auto tags = sihd::util::str::regex_search(html, pattern);

    for (const auto & tag : tags)
    {
        std::string href = extract_attr(tag, url_attr);
        if (href.empty())
            continue;

        ExtractedLink link;
        link.url = resolve_url(href, base_url);
        link.tag = std::string(tag_name);
        link.rel = extract_attr(tag, "rel");

        if (tag_name == "a")
        {
            auto text_matches = sihd::util::str::regex_search(tag, R"(>([^<]*)<)");
            if (!text_matches.empty() && text_matches[0].size() > 2)
                link.text = text_matches[0].substr(1, text_matches[0].size() - 2);
        }
        else if (tag_name == "img")
        {
            link.text = extract_attr(tag, "alt");
        }

        out.push_back(std::move(link));
    }
}

} // namespace

std::vector<ExtractedLink> extract_links(std::string_view html, std::string_view base_url)
{
    std::vector<ExtractedLink> result;
    std::string html_str(html);

    extract_tag(result, html_str, "a", "href", base_url);
    extract_tag(result, html_str, "img", "src", base_url);
    extract_tag(result, html_str, "link", "href", base_url);
    extract_tag(result, html_str, "script", "src", base_url);

    return result;
}

} // namespace sihd::http
