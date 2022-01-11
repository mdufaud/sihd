#include <sihd/http/HttpHeader.hpp>
#include <sihd/util/Logger.hpp>

#ifndef SIHD_HTTP_HEADERS_BUFSIZE
# define SIHD_HTTP_HEADERS_BUFSIZE 4096
#endif

namespace sihd::http
{

LOGGER;

HttpHeader::HttpHeader()
{
    this->set_buffer_size(SIHD_HTTP_HEADERS_BUFSIZE);
}

HttpHeader::~HttpHeader()
{
}

size_t  HttpHeader::size() const
{
    return _ptr == nullptr || _array.cbuf() == nullptr
            ? 0 : _ptr - _array.cbuf();
}

bool    HttpHeader::set_buffer_size(size_t size)
{
    bool ret = _array.resize(size);
    _ptr = _array.buf();
    return ret;
}

void    HttpHeader::set_servername(const std::string & name)
{
    _server_name = name;
}

void    HttpHeader::set_status(uint32_t status)
{
    _status = status;
}

void    HttpHeader::set_content_type(const std::string & type)
{
    _content_type = type;
}

void    HttpHeader::set_charset(const std::string & encoding)
{
    _encoding = encoding;
}

std::string HttpHeader::build_content_type(const std::string & type, const std::string & encoding)
{
    std::string ret = type;
    if (encoding.empty() == false)
        ret += "; charset=" + encoding;
    return ret;
}

void    HttpHeader::set_content_size(size_t len)
{
    _content_size = len;
}

void    HttpHeader::set_common(uint32_t status, const std::string & content_type, size_t content_len)
{
    this->set_status(status);
    this->set_content_type(content_type);
    this->set_content_size(content_len);
}

void    HttpHeader::remove_by_token(enum lws_token_indexes token)
{
    auto it = _headers_token.find(token);
    if (it != _headers_token.end())
        _headers_token.erase(it);
}

void    HttpHeader::remove_by_name(const std::string & name)
{
    auto it = _headers_name.find(name);
    if (it != _headers_name.end())
        _headers_name.erase(it);
}

void    HttpHeader::set_by_token(enum lws_token_indexes token, const std::string & value)
{
    _headers_token[token] = value;
}

void    HttpHeader::set_by_name(const std::string & name, const std::string & value)
{
    _headers_name[name] = value;
}

bool    HttpHeader::finalize(struct lws *wsi)
{
    int rc;
    _ptr = (u_char *)_array.buf();
    u_char *end = (u_char *)_array.buf() + _array.size();

    // STATUS
    rc = lws_add_http_header_status(wsi, _status, &_ptr, end);
    if (rc)
        LOG(error, "HttpHeader: cannot set status");
    // CONTENT TYPE
    std::string type = HttpHeader::build_content_type(_content_type, _encoding);
    rc = lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
                                        (u_char *)type.c_str(),
                                        type.size(), &_ptr, end);
    if (rc)
        LOG(error, "HttpHeader: cannot set content-type");
    // CONTENT LENGTH
    rc = lws_add_http_header_content_length(wsi, _content_size, &_ptr, end);
    if (rc)
        LOG(error, "HttpHeader: cannot set content-length");
    // SERVER NAME
    if (_server_name.empty() == false)
    {
        rc = lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_SERVER,
                                            (u_char *)_server_name.c_str(),
                                            _server_name.size(),
                                            &_ptr, end);
        if (rc)
            LOG(error, "HttpHeader: cannot set server name");
    }
    // TOKENS
    for (const auto & pair: _headers_token)
    {
        rc = lws_add_http_header_by_token(wsi, pair.first, (u_char *)pair.second.c_str(),
                                            pair.second.size(), &_ptr, end);
        if (rc)
            LOG(error, "HttpHeader: cannot set token '" << pair.first << "'");
    }
    // HEADERS NAME
    for (const auto & [name, value]: _headers_name)
    {
        rc = lws_add_http_header_by_name(wsi, (u_char *)name.c_str(),
                                            (u_char *)value.c_str(),
                                            value.size(), &_ptr, end);
        if (rc)
            LOG(error, "HttpHeader: cannot set status '" << name << "'");
    }
    // FINALIZE
    rc = lws_finalize_http_header(wsi, &_ptr, end);
    if (rc)
        LOG(error, "HttpHeader: cannot finalize HTTP headers");
    *_ptr = 0;
    return rc == 0;
}


}