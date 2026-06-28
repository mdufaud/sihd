#include <sihd/json/Json.hpp>

#include <sihd/util/str.hpp>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpStatus.hpp>
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
        case Patch:
            return "PATCH";
        case Head:
            return "HEAD";
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
    else if (str::iequals(type, "patch"))
        return Patch;
    else if (str::iequals(type, "head"))
        return Head;
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

std::optional<std::string_view> HttpRequest::path_param(const std::string & name) const
{
    auto it = _path_params.find(name);
    if (it == _path_params.end())
        return std::nullopt;
    return it->second;
}

std::optional<std::string_view> HttpRequest::query_param(const std::string & name) const
{
    auto it = _query_params.find(name);
    if (it == _query_params.end())
        return std::nullopt;
    return it->second;
}

std::optional<std::string_view> HttpRequest::cookie(const std::string & name) const
{
    auto it = _cookies.find(name);
    if (it == _cookies.end())
        return std::nullopt;
    return it->second;
}

void HttpRequest::set_cookie(std::string_view name, std::string_view value)
{
    _cookies.emplace(std::string(name), std::string(value));
}

sihd::json::Json HttpRequest::content_as_json() const
{
    if (!_array)
        return sihd::json::Json();
    const auto *buf = reinterpret_cast<const char *>(_array.buf());
    std::string_view content(buf, _array.size());
    return sihd::json::Json::parse(content, false);
}

std::string HttpRequest::type_str() const
{
    return HttpRequest::type_str(_request_type);
}

bool HttpRequest::has_content() const
{
    return _array;
}

std::optional<HttpRequest> HttpRequest::from_string(std::string_view raw)
{
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string_view::npos)
        return std::nullopt;

    std::string_view header_section = raw.substr(0, header_end);
    std::string_view body = raw.substr(header_end + 4);

    auto lines = str::split(header_section, "\r\n");
    if (lines.empty())
        return std::nullopt;

    auto request_parts = str::split(lines[0], " ");
    if (request_parts.size() < 3)
        return std::nullopt;

    auto method = type_from_str(request_parts[0]);
    if (method == None)
        return std::nullopt;

    if (!str::starts_with(request_parts[2], "HTTP/1."))
        return std::nullopt;

    std::string_view full_path = request_parts[1];
    std::string_view path = full_path;
    std::unordered_map<std::string, std::string> query_params;

    if (auto qpos = full_path.find('?'); qpos != std::string_view::npos)
    {
        path = full_path.substr(0, qpos);
        std::string_view query_str = full_path.substr(qpos + 1);
        for (const auto & param : str::split(query_str, "&"))
        {
            auto [key, value] = str::split_pair(param, "=");
            if (!key.empty())
                query_params[key] = value;
        }
    }

    HttpRequest req(std::string(path), method);

    if (!query_params.empty())
        req.set_query_params(std::move(query_params));

    for (size_t i = 1; i < lines.size(); ++i)
        req._http_header.add_header_from_str(lines[i]);

    if (!body.empty())
        req.set_content({body.data(), body.size()});

    return req;
}

std::string HttpRequest::to_string() const
{
    std::string result;
    result += type_str();
    result += " ";
    result += _url;

    if (!_query_params.empty())
    {
        result += "?";
        bool first = true;
        for (const auto & [key, value] : _query_params)
        {
            if (!first)
                result += "&";
            result += key;
            result += "=";
            result += value;
            first = false;
        }
    }

    result += " HTTP/1.1\r\n";

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

    result += "\r\n";

    if (_array)
        result.append(reinterpret_cast<const char *>(_array.buf()), _array.size());

    return result;
}

} // namespace sihd::http