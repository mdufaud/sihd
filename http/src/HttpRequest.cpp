#include <nlohmann/json.hpp>

#include <sihd/util/str.hpp>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::http
{

SIHD_LOGGER;

using namespace sihd::util;

std::string HttpRequest::type_str(HttpRequest::RequestType type)
{
    switch (type)
    {
        case Get:
            return "GET";
        case Post:
            return "POST";
        case Put:
            return "PUT";
        case Delete:
            return "DELETE";
        default:
            return "None";
    }
}

HttpRequest::RequestType HttpRequest::type_from_str(std::string_view type)
{
    if (str::iequals(type, "get"))
        return Get;
    else if (str::iequals(type, "post"))
        return Post;
    else if (str::iequals(type, "put"))
        return Put;
    else if (str::iequals(type, "delete"))
        return Delete;
    return None;
}

HttpRequest::HttpRequest(std::string_view url, RequestType request_type)
{
    _url = url;
    _request_type = request_type;
}

HttpRequest::HttpRequest(std::string_view url,
                         const std::vector<std::string> & uri_args,
                         RequestType request_type):
    HttpRequest(url, request_type)
{
    _uri_args_lst = uri_args;
}

HttpRequest::~HttpRequest() = default;

void HttpRequest::set_content(sihd::util::ArrCharView data)
{
    _array.from_bytes(data);
}

void HttpRequest::set_client_ip(const std::string & ip)
{
    _ip = ip;
}

nlohmann::json HttpRequest::content_as_json() const
{
    if (!_array)
        return {};
    return nlohmann::json::parse(_array.buf(), _array.buf() + _array.size(), nullptr, false);
}

std::string HttpRequest::type_str() const
{
    return HttpRequest::type_str(_request_type);
}

bool HttpRequest::has_content() const
{
    return _array;
}

} // namespace sihd::http