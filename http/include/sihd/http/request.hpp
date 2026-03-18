#ifndef __SIHD_HTTP_REQUEST_HPP__
#define __SIHD_HTTP_REQUEST_HPP__

#include <future>

#include <sihd/http/CurlOptions.hpp>
#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>

#include <sihd/util/ArrayView.hpp>

namespace sihd::http
{

using OptionalHttpResponse = std::optional<HttpResponse>;
using FutureHttpResponse = std::future<OptionalHttpResponse>;

std::optional<HttpResponse> get(std::string_view url, const CurlOptions & curlopt = CurlOptions::none());

std::optional<HttpResponse> post(std::string_view url,
                                 sihd::util::ArrCharView data_view,
                                 const CurlOptions & curlopt = CurlOptions::none());

std::optional<HttpResponse>
    put(std::string_view url, std::string_view file_path, const CurlOptions & curlopt = CurlOptions::none());

std::optional<HttpResponse> del(std::string_view url, const CurlOptions & curlopt = CurlOptions::none());

std::optional<HttpResponse> options(std::string_view url, const CurlOptions & curlopt = CurlOptions::none());

std::future<std::optional<HttpResponse>> async_get(std::string_view url,
                                                   const CurlOptions & curlopt = CurlOptions::none());

std::future<std::optional<HttpResponse>> async_post(std::string_view url,
                                                    sihd::util::ArrCharView data_view,
                                                    const CurlOptions & curlopt = CurlOptions::none());

std::future<std::optional<HttpResponse>> async_put(std::string_view url,
                                                   std::string_view file_path,
                                                   const CurlOptions & curlopt = CurlOptions::none());

std::future<std::optional<HttpResponse>> async_del(std::string_view url,
                                                   const CurlOptions & curlopt = CurlOptions::none());

std::future<std::optional<HttpResponse>> async_options(std::string_view url,
                                                       const CurlOptions & curlopt = CurlOptions::none());

} // namespace sihd::http

#endif
