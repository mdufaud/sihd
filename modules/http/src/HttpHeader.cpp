#include <sihd/http/HttpHeader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include "lws.hpp"

namespace sihd::http
{

using namespace sihd::util;

namespace
{
std::string format_hdr_name(std::string_view entry)
{
    std::string ret;
    if (str::ends_with(entry, ":"))
        ret = entry;
    else
        ret = fmt::format("{}:", entry);
    str::to_lower(ret);
    return ret;
}

// std::nullopt when the header was never set, which an empty value cannot tell
std::optional<std::string_view> header_value(const HttpHeader::HeaderMap & headers, const std::string & name)
{
    const auto it = headers.find(name);
    if (it == headers.end())
        return std::nullopt;
    return it->second;
}

size_t param_end(std::string_view value, size_t start)
{
    bool quoted = false;
    for (size_t i = start; i < value.size(); ++i)
    {
        const char c = value[i];
        if (c == '"')
            quoted = !quoted;
        else if (c == ';' && quoted == false)
            return i;
    }
    return value.size();
}
} // namespace

SIHD_LOGGER;

HttpHeader::HttpHeader() = default;

HttpHeader::HttpHeader(HeaderMap && headers): _headers(headers) {}

HttpHeader::~HttpHeader() = default;

HttpHeader & HttpHeader::set_server(std::string_view name)
{
    return this->set_header("server:", name);
}

HttpHeader & HttpHeader::set_content_type(std::string_view type)
{
    return this->set_header("content-type:", type);
}

HttpHeader & HttpHeader::set_content_type(std::string_view type, std::string_view charset)
{
    std::string content = fmt::format("{}; charset={}", type, charset);
    return this->set_content_type(content);
}

HttpHeader & HttpHeader::set_accept_charset(std::string_view charset)
{
    return this->set_header("accept-charset:", charset);
}

HttpHeader & HttpHeader::set_content_length(size_t len)
{
    return this->set_header("content-length:", std::to_string(len));
}

HttpHeader & HttpHeader::set_accept(std::string_view mime_type)
{
    return this->set_header("accept:", mime_type);
}

std::optional<size_t> HttpHeader::content_length() const
{
    const std::optional<std::string_view> value = header_value(_headers, "content-length:");
    if (value.has_value() == false)
        return std::nullopt;
    // str::to_unsigned wraps signed input around: only RFC digits are a valid length
    const std::string_view len = util::str::trim(*value);
    if (len.empty())
        return std::nullopt;
    for (const char c : len)
    {
        if (c < '0' || c > '9')
            return std::nullopt;
    }
    return util::str::convert_from_string<size_t>(len);
}

std::optional<std::string_view> HttpHeader::accept_charset() const
{
    return header_value(_headers, "accept-charset:");
}

std::optional<std::string_view> HttpHeader::content_type() const
{
    return header_value(_headers, "content-type:");
}

std::optional<std::string_view> HttpHeader::server() const
{
    return header_value(_headers, "server:");
}

HttpHeader & HttpHeader::remove_header(const std::string & name)
{
    auto it = _headers.find(format_hdr_name(name));
    if (it != _headers.end())
        _headers.erase(it);
    return *this;
}

HttpHeader & HttpHeader::set_header(const std::string & header_name, std::string_view value)
{
    std::string lower_name = format_hdr_name(header_name);
    _headers.insert_or_assign(std::move(lower_name), std::string(value));
    return *this;
}

bool HttpHeader::add_header_from_str(std::string_view header)
{
    if (util::str::ends_with(header, "\n"))
        header.remove_suffix(1);
    const size_t colon = header.find(':');
    if (colon == std::string_view::npos)
        return false;
    const std::string_view value = util::str::trim(header.substr(colon + 1));
    if (value.empty())
        return false;
    this->set_header(std::string(util::str::trim(header.substr(0, colon))), value);
    return true;
}

std::optional<std::string> header_param(std::string_view value, std::string_view param)
{
    size_t start = 0;
    while (start < value.size())
    {
        const size_t end = param_end(value, start);
        const auto [name, raw_value] = util::str::split_pair(value.substr(start, end - start), "=");
        if (util::str::iequals(util::str::trim(name), param))
        {
            std::string found(util::str::unquote(raw_value));
            if (found.empty() == false)
                return found;
        }
        start = end + 1;
    }
    return std::nullopt;
}

HttpHeader & HttpHeader::set_headers(HeaderMap && headers)
{
    for (const auto & [name, value] : headers)
    {
        this->set_header(name, value);
    }
    return *this;
}

const std::string & HttpHeader::get(const std::string & header_name) const
{
    std::string lower_name = format_hdr_name(header_name);
    return _headers.at(lower_name);
}

std::string_view HttpHeader::find(const std::string & header_name) const
{
    std::string lower_name = format_hdr_name(header_name);
    const auto it = _headers.find(lower_name);
    if (it != _headers.end())
        return it->second;
    return {};
}

} // namespace sihd::http