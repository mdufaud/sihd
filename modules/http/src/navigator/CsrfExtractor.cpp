#include <string>
#include <vector>

#include <sihd/http/navigator/CsrfExtractor.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

namespace sihd::http
{

SIHD_LOGGER;

namespace
{

static const std::vector<std::string_view> KNOWN_CSRF_FIELDS = {
    "_csrf",
    "authenticity_token",
    "__RequestVerificationToken",
    "_token",
    "csrf_token",
    "csrfmiddlewaretoken",
};

std::string regex_escape(std::string_view str)
{
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str)
    {
        if (std::string_view(R"(\^$.[]({})*+?|)").find(c) != std::string_view::npos)
            result += '\\';
        result += c;
    }
    return result;
}

std::string extract_hidden_field(std::string_view html, std::string_view field_name)
{
    std::string html_str(html);
    std::string field = regex_escape(field_name);

    // try: name attribute before value attribute
    std::string pattern = fmt::format(R"(<input[^>]+name\s*=\s*["']?{}["']?[^>]+>)", field);
    auto tags = sihd::util::str::regex_search(html_str, pattern);

    if (tags.empty())
    {
        // try: value attribute before name attribute
        pattern = fmt::format(R"(<input[^>]+value\s*=\s*["'][^"']*["'][^>]+name\s*=\s*["']?{}["']?[^>]*>)", field);
        tags = sihd::util::str::regex_search(html_str, pattern);
        if (tags.empty())
            return {};
    }

    // extract value="..." from the matched tag
    auto val_matches = sihd::util::str::regex_search(tags[0], R"(value\s*=\s*["'][^"']*["'])");
    if (val_matches.empty())
        return {};

    auto [_, raw] = sihd::util::str::split_pair_view(val_matches[0], "=");
    raw = sihd::util::str::trim(raw);
    if (raw.size() >= 2 && (raw.front() == '"' || raw.front() == '\''))
        return std::string(raw.substr(1, raw.size() - 2));
    return std::string(raw);
}

} // namespace

CsrfResult csrf_extract_from_html(std::string_view html, std::optional<std::string_view> hint)
{
    if (hint.has_value())
    {
        std::string value = extract_hidden_field(html, *hint);
        if (!value.empty())
            return {std::string(*hint), std::move(value)};
        SIHD_LOG(debug, "CsrfExtractor: field '{}' not found in HTML", *hint);
        return {};
    }

    for (std::string_view name : KNOWN_CSRF_FIELDS)
    {
        std::string value = extract_hidden_field(html, name);
        if (!value.empty())
            return {std::string(name), std::move(value)};
    }

    return {};
}

} // namespace sihd::http
