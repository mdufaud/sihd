#include <sihd/json/Json.hpp>

#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

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

bool HttpResponse::set_json_content(const sihd::json::Json & data)
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

std::optional<HttpResponse> HttpResponse::from_string(std::string_view raw)
{
    using namespace sihd::util;

    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string_view::npos)
        return std::nullopt;

    std::string_view header_section = raw.substr(0, header_end);
    std::string_view body = raw.substr(header_end + 4);

    auto lines = str::split(header_section, "\r\n");
    if (lines.empty())
        return std::nullopt;

    auto status_parts = str::split(lines[0], " ");
    if (status_parts.size() < 2)
        return std::nullopt;

    if (!str::starts_with(status_parts[0], "HTTP/1."))
        return std::nullopt;

    uint32_t status;
    if (!str::convert_from_string(status_parts[1], status))
        return std::nullopt;

    HttpResponse resp;
    resp.set_status(status);

    for (size_t i = 1; i < lines.size(); ++i)
    {
        if (str::starts_with(lines[i], "Set-Cookie: ") || str::starts_with(lines[i], "set-cookie: "))
        {
            std::string_view cookie_val = std::string_view(lines[i]).substr(lines[i].find(": ") + 2);
            resp._cookies.emplace_back(std::string(cookie_val));
        }
        else
        {
            resp._http_header.add_header_from_str(lines[i]);
        }
    }

    if (!body.empty())
    {
        resp._array.resize(body.size());
        resp._array.copy_from_bytes(body.data(), body.size());
    }

    return resp;
}

std::string HttpResponse::to_string() const
{
    using namespace sihd::util;

    std::string result;
    result += "HTTP/1.1 ";
    result += std::to_string(_status);
    result += " ";
    result += HttpStatus::to_string(_status);
    result += "\r\n";

    for (const auto & [name, value] : _http_header.headers())
    {
        std::string_view hdr_name = name;
        if (str::ends_with(hdr_name, ":"))
            hdr_name.remove_suffix(1);
        result += hdr_name;
        result += ": ";
        result += value;
        result += "\r\n";
    }

    for (const auto & cookie : _cookies)
    {
        result += "Set-Cookie: ";
        result += cookie;
        result += "\r\n";
    }

    result += "\r\n";

    if (_array.size() > 0)
        result.append(reinterpret_cast<const char *>(_array.buf()), _array.size());

    return result;
}

} // namespace sihd::http