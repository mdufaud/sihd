#include <sihd/http/HttpRequest.hpp>
#include <sihd/util/Logger.hpp>
#include <algorithm>

namespace sihd::http
{

SIHD_LOGGER;

HttpRequest::HttpRequest(const std::string & path, RequestType request_type)
{
    _array_content = nullptr;
    _path = path;
    _request_type = request_type;
}

HttpRequest::HttpRequest(const std::string & path, const std::vector<std::string> & uri_args, RequestType request_type):
    HttpRequest(path, request_type)
{
    _uri_args_lst = uri_args;
}

HttpRequest::~HttpRequest()
{
}

nlohmann::json  HttpRequest::content_as_json() const
{
    nlohmann::json json = nlohmann::json::parse(_array_content->buf(),
                                                _array_content->buf() + _array_content->size(),
                                                nullptr, false);
    return json;
}

void    HttpRequest::set_content(sihd::util::IArray *array)
{
    _array_content = array;
}

std::string HttpRequest::request_to_string(HttpRequest::RequestType type)
{
    switch (type)
    {
        case GET:
            return "GET";
        case POST:
            return "POST";
        case PUT:
            return "PUT";
        case REQ_DELETE:
            return "DELETE";
        default:
            return "NONE";
    }
}

HttpRequest::RequestType    HttpRequest::request_from_string(std::string type)
{
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    if (type == "get")
        return GET;
    else if (type == "post")
        return POST;
    else if (type == "put")
        return PUT;
    else if (type == "delete")
        return REQ_DELETE;
    return NONE;
}

}