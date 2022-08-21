#include <libwebsockets.h>

#include <sihd/util/Logger.hpp>

#include <sihd/http/HttpResponse.hpp>

namespace sihd::http
{

SIHD_LOGGER;

HttpResponse::HttpResponse(Mime *mimes): _mime_ptr(mimes)
{
    _http_header.set_status(HTTP_STATUS_OK);
    _http_header.set_charset("utf-8");
}

HttpResponse::~HttpResponse()
{
}

void    HttpResponse::set_content_type(std::string_view mime_type)
{
    _http_header.set_content_type(mime_type);
}

void    HttpResponse::set_content_type_from_extension(const std::string & type)
{
    if (_mime_ptr != nullptr)
        this->set_content_type(_mime_ptr->get(type));
}


bool    HttpResponse::set_json_content(const nlohmann::json & data)
{
    this->_set_mime_type_if_not_set(Mime::MIME_APPLICATION_JSON);
    std::string json_string = data.dump();
    return this->set_content({json_string.c_str(), json_string.size()});
}

bool    HttpResponse::set_plain_content(std::string_view str)
{
    this->_set_mime_type_if_not_set(Mime::MIME_TEXT_PLAIN);
    return this->set_content(str);
}

bool    HttpResponse::set_byte_content(sihd::util::ArrViewByte data)
{
    this->_set_mime_type_if_not_set(Mime::MIME_APPLICATION_OCTET);
    return this->set_content(data);
}

bool    HttpResponse::set_content(sihd::util::ArrViewChar data)
{
    if (_array.resize(data.byte_size()) == false)
        return false;
    _array.copy_from_bytes(data.buf(), data.byte_size());
    return true;
}

void    HttpResponse::_set_mime_type_if_not_set(const char *type)
{
    if (_http_header.content_type().empty())
        this->set_content_type(type);
}

}