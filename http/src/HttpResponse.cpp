#include <nlohmann/json.hpp>

#include <sihd/util/Logger.hpp>

#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/HttpStatus.hpp>

namespace sihd::http
{

SIHD_LOGGER;

HttpResponse::HttpResponse(Mime *mimes): _status(HttpStatus::Ok), _mime_ptr(mimes)
{
    _http_header.set_accept_charset("utf-8");
}

HttpResponse::~HttpResponse() = default;

void HttpResponse::set_content_type(std::string_view mime_type)
{
    _http_header.set_content_type(mime_type, "utf-8");
}

void HttpResponse::set_content_type_from_extension(const std::string & type)
{
    if (_mime_ptr != nullptr)
        this->set_content_type(_mime_ptr->get(type));
}

bool HttpResponse::set_json_content(const nlohmann::json & data)
{
    this->_set_mime_type_if_not_set(Mime::MIME_APPLICATION_JSON);
    std::string json_string = data.dump();
    return this->set_content({json_string.c_str(), json_string.size()});
}

bool HttpResponse::set_plain_content(std::string_view str)
{
    this->_set_mime_type_if_not_set(Mime::MIME_TEXT_PLAIN);
    return this->set_content(str);
}

bool HttpResponse::set_byte_content(sihd::util::ArrByteView data)
{
    this->_set_mime_type_if_not_set(Mime::MIME_APPLICATION_OCTET);
    return this->set_content(data);
}

void HttpResponse::set_status(uint32_t status)
{
    _status = status;
}

bool HttpResponse::set_content(sihd::util::ArrCharView data)
{
    if (_array.resize(data.byte_size()) == false)
        return false;
    _array.copy_from_bytes(data.buf(), data.byte_size());
    return true;
}

void HttpResponse::_set_mime_type_if_not_set(const char *type)
{
    if (_http_header.content_type().empty())
        this->set_content_type(type);
}

void HttpResponse::set_stream_provider(StreamProvider provider)
{
    _stream_provider = std::move(provider);
}

void HttpResponse::set_cookie(std::string_view name, std::string_view value, std::string_view options)
{
    std::string cookie_str = fmt::format("{}={}", name, value);
    if (!options.empty())
    {
        cookie_str += "; ";
        cookie_str += options;
    }
    _cookies.emplace_back(std::move(cookie_str));
}

} // namespace sihd::http