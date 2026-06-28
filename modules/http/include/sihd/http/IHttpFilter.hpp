#ifndef __SIHD_HTTP_IHTTPFILTER_HPP__
#define __SIHD_HTTP_IHTTPFILTER_HPP__

#include <string>
#include <string_view>
#include <unordered_map>

#include <sihd/http/HttpRequest.hpp>

namespace sihd::http
{

struct HttpFilterInfo
{
        std::string_view uri;
        HttpRequest::RequestType method;
        std::string client_ip;
        std::unordered_map<std::string, std::string> headers;
};

class IHttpFilter
{
    public:
        virtual ~IHttpFilter() = default;
        // return true to allow request, false to reject with 403
        virtual bool on_filter_connection(const HttpFilterInfo & info) = 0;
};

} // namespace sihd::http

#endif
