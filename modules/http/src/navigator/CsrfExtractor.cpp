#include <string>
#include <vector>

#include <sihd/http/navigator/CsrfExtractor.hpp>
#include <sihd/util/Logger.hpp>

#include "HtmlScanner.hpp"

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

std::string extract_hidden_field(std::string_view html, std::string_view field_name)
{
    std::string value;
    htmlscan::scan_tags(html, [&](const htmlscan::Tag & tag) {
        if (!value.empty() || tag.name != "input" || tag.attribute("name") != field_name)
            return;
        value = std::string(tag.attribute("value").value_or(std::string_view()));
    });
    return value;
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
