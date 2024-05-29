#include <libwebsockets.h>

#include <sihd/http/HttpHeader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>

namespace sihd::http
{

SIHD_LOGGER;

std::string HttpHeader::build_content_type(std::string_view type, std::string_view charset)
{
    std::string ret(type.data(), type.size());
    if (charset.empty() == false)
    {
        ret.append("; charset=");
        ret.append(charset.data(), charset.size());
    }
    return ret;
}

HttpHeader::HttpHeader() {}

HttpHeader::HttpHeader(HeaderMap && headers): _headers(headers) {}

HttpHeader::~HttpHeader() {}

HttpHeader & HttpHeader::set_server(std::string_view name)
{
    return this->set_header("Server", name);
}

HttpHeader & HttpHeader::set_content_type(std::string_view type)
{
    return this->set_header("Content-Type", type);
}

HttpHeader & HttpHeader::set_accept_charset(std::string_view charset)
{
    return this->set_header("Accept-Charset", charset);
}

HttpHeader & HttpHeader::set_content_length(size_t len)
{
    return this->set_header("Content-Length", std::to_string(len));
}

HttpHeader & HttpHeader::set_accept(std::string_view mime_type)
{
    return this->set_header("Accept", mime_type);
}

size_t HttpHeader::content_length() const
{
    std::string_view value_str
        = util::container::get_or<decltype(_headers), std::string, std::string_view>(_headers, "Content-Length", "");
    size_t value;
    if (util::str::convert_from_string(value_str, value) == false)
        return 0;
    return value;
}

std::string_view HttpHeader::accept_charset() const
{
    return util::container::get_or<decltype(_headers), std::string, std::string_view>(_headers, "Accept-Charset", "");
}

std::string_view HttpHeader::content_type() const
{
    return util::container::get_or<decltype(_headers), std::string, std::string_view>(_headers, "Content-Type", "");
}

std::string_view HttpHeader::server() const
{
    return util::container::get_or<decltype(_headers), std::string, std::string_view>(_headers, "Server", "");
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

HttpHeader & HttpHeader::set_header(const std::string & name, std::string_view value)
{
    _headers.emplace(name, value);
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
        _headers[name] = value;
    }
    return *this;
}

std::string_view HttpHeader::find(const std::string & header_name) const
{
    const auto it = _headers.find(header_name);
    if (it != _headers.end())
        return it->second;
    return {};
}

} // namespace sihd::http