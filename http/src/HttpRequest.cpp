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
        case Options:
            return "OPTIONS";
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
    else if (str::iequals(type, "options"))
        return Options;
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

void HttpRequest::set_auth_user(std::string_view user)
{
    _auth_user = user;
}

void HttpRequest::set_auth_token(std::string_view token)
{
    _auth_token = token;
}

void HttpRequest::set_path_params(std::unordered_map<std::string, std::string> && params)
{
    _path_params = std::move(params);
}

void HttpRequest::set_query_params(std::unordered_map<std::string, std::string> && params)
{
    _query_params = std::move(params);
}

std::optional<std::string_view> HttpRequest::path_param(std::string_view name) const
{
    auto it = _path_params.find(std::string(name));
    if (it == _path_params.end())
        return std::nullopt;
    return it->second;
}

std::optional<std::string_view> HttpRequest::query_param(std::string_view name) const
{
    auto it = _query_params.find(std::string(name));
    if (it == _query_params.end())
        return std::nullopt;
    return it->second;
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