#include <libwebsockets.h>

#include <sihd/http/HttpHeader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/str.hpp>

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
} // namespace

SIHD_LOGGER;

HttpHeader::HttpHeader() {}

HttpHeader::HttpHeader(HeaderMap && headers): _headers(headers) {}

HttpHeader::~HttpHeader() {}

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

size_t HttpHeader::content_length() const
{
    std::string_view value_str
        = util::container::get_or<decltype(_headers), std::string, std::string_view>(_headers, "content-length:", "");
    size_t value;
    if (util::str::convert_from_string(value_str, value) == false)
        return 0;
    return value;
}

std::string_view HttpHeader::accept_charset() const
{
    return util::container::get_or<decltype(_headers), std::string, std::string_view>(_headers, "accept-charset:", "");
}

std::string_view HttpHeader::content_type() const
{
    return util::container::get_or<decltype(_headers), std::string, std::string_view>(_headers, "content-type:", "");
}

std::string_view HttpHeader::server() const
{
    return util::container::get_or<decltype(_headers), std::string, std::string_view>(_headers, "server:", "");
}

HttpHeader & HttpHeader::remove_header(const std::string & name)
{
    auto it = _headers.find(name);
    if (it != _headers.end())
        _headers.erase(it);
    return *this;
}

HttpHeader & HttpHeader::remove_header(const unsigned char *name)
{
    if (name == nullptr)
        throw std::runtime_error("header is null");
    this->remove_header((const char *)name);
    return *this;
}

HttpHeader & HttpHeader::set_header(const unsigned char *name, std::string_view value)
{
    if (name == nullptr)
        throw std::runtime_error("header is null");
    this->set_header((const char *)name, value);
    return *this;
}

HttpHeader & HttpHeader::set_header(const std::string & header_name, std::string_view value)
{
    std::string lower_name = format_hdr_name(header_name);
    _headers.emplace(std::move(lower_name), value);
    return *this;
}

bool HttpHeader::add_header_from_str(std::string_view header)
{
    if (util::str::ends_with(header, "\n"))
        header.remove_suffix(1);
    const size_t pos_before = header.find_first_of(": ");
    const size_t pos_after = pos_before + 2;
    if (pos_before == std::string_view::npos || pos_after >= header.size())
        return false;
    this->set_header(std::string(header.data(), pos_before),
                     std::string(header.data() + pos_after, header.size() - pos_after));
    return true;
}

HttpHeader & HttpHeader::set_headers(HeaderMap && headers)
{
    for (const auto & [name, value] : headers)
    {
        this->set_header(name, value);
    }
    return *this;
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