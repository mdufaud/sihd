#include <libwebsockets.h>

#include <sihd/util/Logger.hpp>

#include <sihd/http/HttpHeader.hpp>

namespace sihd::http
{

SIHD_LOGGER;

std::string HttpHeader::build_content_type(std::string_view type, std::string_view encoding)
{
    std::string ret(type.data(), type.size());
    if (encoding.empty() == false)
    {
        ret.append("; charset=");
        ret.append(encoding.data(), encoding.size());
    }
    return ret;
}

HttpHeader::HttpHeader()
{
}

HttpHeader::~HttpHeader()
{
}

HttpHeader &    HttpHeader::set_server_name(std::string_view name)
{
    _server_name = name;
    return *this;
}

HttpHeader &    HttpHeader::set_status(uint32_t status)
{
    _status = status;
    return *this;
}

HttpHeader &    HttpHeader::set_content_type(std::string_view type)
{
    _content_type = type;
    return *this;
}

HttpHeader &    HttpHeader::set_charset(std::string_view encoding)
{
    _encoding = encoding;
    return *this;
}

HttpHeader &    HttpHeader::set_content_size(size_t len)
{
    _content_size = len;
    return *this;
}

HttpHeader &    HttpHeader::set_status_content(uint32_t status, std::string_view content_type, size_t content_len)
{
    this->set_status(status);
    this->set_content_type(content_type);
    this->set_content_size(content_len);
    return *this;
}

HttpHeader &    HttpHeader::remove_header(const std::string & name)
{
    auto it = _headers.find(name);
    if (it != _headers.end())
        _headers.erase(it);
    return *this;
}

HttpHeader &    HttpHeader::remove_header(const unsigned char *name)
{
    if (name == nullptr)
        throw std::runtime_error("header is null");
    this->remove_header((const char *)name);
    return *this;
}

HttpHeader &    HttpHeader::set_header(const unsigned char *name, std::string_view value)
{
    if (name == nullptr)
        throw std::runtime_error("header is null");
    this->set_header(std::string((const char *)name), value);
    return *this;
}

HttpHeader &    HttpHeader::set_header(const std::string & name, std::string_view value)
{
    _headers[name] = value;
    return *this;
}

}