#ifndef __SIHD_HTTP_HTTPSENDER_HPP__
#define __SIHD_HTTP_HTTPSENDER_HPP__

#include <sihd/http/HttpOptions.hpp>
#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/util/ArrayView.hpp>

namespace sihd::http
{

struct HttpSender
{
        static std::optional<HttpResponse> get(std::string_view url, const CurlOptions & options = CurlOptions::none());
        static std::optional<HttpResponse> post(std::string_view url,
                                                sihd::util::ArrCharView data_view,
                                                const CurlOptions & options = CurlOptions::none());
        static std::optional<HttpResponse>
            put(std::string_view url, std::string_view file_path, const CurlOptions & options = CurlOptions::none());
        static std::optional<HttpResponse> del(std::string_view url, const CurlOptions & options = CurlOptions::none());
};

} // namespace sihd::http

#endif
