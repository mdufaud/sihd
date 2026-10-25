#ifndef __SIHD_HTTP_REQUEST_HPP__
#define __SIHD_HTTP_REQUEST_HPP__

#include <future>
#include <string_view>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/RequestOptions.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::http
{

using FutureHttpResponse = std::future<sihd::util::Result<HttpResponse>>;

sihd::util::Result<HttpResponse> get(std::string_view url, const RequestOptions & options = RequestOptions::none());

sihd::util::Result<HttpResponse> post(std::string_view url,
                                      sihd::util::ArrCharView data_view,
                                      const RequestOptions & options = RequestOptions::none());

sihd::util::Result<HttpResponse>
    put(std::string_view url, const std::string & file_path, const RequestOptions & options = RequestOptions::none());

sihd::util::Result<HttpResponse> del(std::string_view url, const RequestOptions & options = RequestOptions::none());

sihd::util::Result<HttpResponse> options(std::string_view url, const RequestOptions & options = RequestOptions::none());

sihd::util::Result<HttpResponse> patch(std::string_view url,
                                       sihd::util::ArrCharView data_view,
                                       const RequestOptions & options = RequestOptions::none());

sihd::util::Result<HttpResponse> head(std::string_view url, const RequestOptions & options = RequestOptions::none());

std::future<sihd::util::Result<HttpResponse>> async_get(const std::string & url,
                                                        const RequestOptions & options = RequestOptions::none());

std::future<sihd::util::Result<HttpResponse>> async_post(const std::string & url,
                                                         sihd::util::ArrCharView data_view,
                                                         const RequestOptions & options = RequestOptions::none());

std::future<sihd::util::Result<HttpResponse>> async_put(const std::string & url,
                                                        const std::string & file_path,
                                                        const RequestOptions & options = RequestOptions::none());

std::future<sihd::util::Result<HttpResponse>> async_del(const std::string & url,
                                                        const RequestOptions & options = RequestOptions::none());

std::future<sihd::util::Result<HttpResponse>> async_options(const std::string & url,
                                                            const RequestOptions & options = RequestOptions::none());

std::future<sihd::util::Result<HttpResponse>> async_patch(const std::string & url,
                                                          sihd::util::ArrCharView data_view,
                                                          const RequestOptions & options = RequestOptions::none());

std::future<sihd::util::Result<HttpResponse>> async_head(const std::string & url,
                                                         const RequestOptions & options = RequestOptions::none());

} // namespace sihd::http

#endif
