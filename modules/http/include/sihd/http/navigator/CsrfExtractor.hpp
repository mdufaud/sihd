#ifndef __SIHD_HTTP_CSRFEXTRACTOR_HPP__
#define __SIHD_HTTP_CSRFEXTRACTOR_HPP__

#include <optional>
#include <string>
#include <string_view>

namespace sihd::http
{

struct CsrfResult
{
        std::string field_name;
        std::string value;
};

// Scan an HTML body for a CSRF hidden input field: the hint field name when provided, otherwise
// common names (_csrf, authenticity_token, __RequestVerificationToken, _token, csrf_token...)
CsrfResult csrf_extract_from_html(std::string_view html, std::optional<std::string_view> hint = std::nullopt);

} // namespace sihd::http

#endif
