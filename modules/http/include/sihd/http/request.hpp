#ifndef __SIHD_HTTP_REQUEST_HPP__
#define __SIHD_HTTP_REQUEST_HPP__

#include <future>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/RequestOptions.hpp>
#include <sihd/util/ArrayView.hpp>

namespace sihd::http
{

using OptionalHttpResponse = std::optional<HttpResponse>;
using FutureHttpResponse = std::future<OptionalHttpResponse>;

std::optional<HttpResponse> get(std::string_view url, const RequestOptions & options = RequestOptions::none());

std::optional<HttpResponse> post(std::string_view url,
                                 sihd::util::ArrCharView data_view,
                                 const RequestOptions & options = RequestOptions::none());

std::optional<HttpResponse>
    put(std::string_view url, std::string_view file_path, const RequestOptions & options = RequestOptions::none());

std::optional<HttpResponse> del(std::string_view url, const RequestOptions & options = RequestOptions::none());

std::optional<HttpResponse> options(std::string_view url, const RequestOptions & options = RequestOptions::none());

std::optional<HttpResponse> patch(std::string_view url,
                                  sihd::util::ArrCharView data_view,
                                  const RequestOptions & options = RequestOptions::none());

std::optional<HttpResponse> head(std::string_view url, const RequestOptions & options = RequestOptions::none());

std::future<std::optional<HttpResponse>> async_get(std::string_view url,
                                                   const RequestOptions & options = RequestOptions::none());

std::future<std::optional<HttpResponse>> async_post(std::string_view url,
                                                    sihd::util::ArrCharView data_view,
                                                    const RequestOptions & options = RequestOptions::none());

std::future<std::optional<HttpResponse>> async_put(std::string_view url,
                                                   std::string_view file_path,
                                                   const RequestOptions & options = RequestOptions::none());

std::future<std::optional<HttpResponse>> async_del(std::string_view url,
                                                   const RequestOptions & options = RequestOptions::none());

std::future<std::optional<HttpResponse>> async_options(std::string_view url,
                                                       const RequestOptions & options = RequestOptions::none());

std::future<std::optional<HttpResponse>> async_patch(std::string_view url,
                                                     sihd::util::ArrCharView data_view,
                                                     const RequestOptions & options = RequestOptions::none());

std::future<std::optional<HttpResponse>> async_head(std::string_view url,
                                                    const RequestOptions & options = RequestOptions::none());

} // namespace sihd::http

#endif
