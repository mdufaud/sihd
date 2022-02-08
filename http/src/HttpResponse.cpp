#include <sihd/http/HttpResponse.hpp>
#include <sihd/util/Logger.hpp>

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

void    HttpResponse::set_content_type(const std::string & mime_type)
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
    if (_array.resize(json_string.size()) == false)
        return false;
    _array.copy_from_bytes((const uint8_t *)json_string.c_str(), json_string.size());
    return true;
}

bool    HttpResponse::set_content(const std::string & str)
{
    this->_set_mime_type_if_not_set(Mime::MIME_TEXT_PLAIN);
    if (_array.resize(str.size()) == false)
        return false;
    _array.copy_from_bytes((const uint8_t *)str.c_str(), str.size());
    return true;
}

bool    HttpResponse::set_content(const sihd::util::IArray & data)
{
    this->_set_mime_type_if_not_set(Mime::MIME_APPLICATION_OCTET);
    if (_array.resize(data.byte_size()) == false)
        return false;
    _array.copy_from_bytes(data.cbuf(), data.byte_size());
    return true;
}

void    HttpResponse::_set_mime_type_if_not_set(const char *type)
{
    if (_http_header.content_type().empty())
        this->set_content_type(type);
}

}