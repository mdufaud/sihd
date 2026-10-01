#ifndef __SIHD_HTTP_REQUEST_HPP__
#define __SIHD_HTTP_REQUEST_HPP__

#include <expected>
#include <future>
#include <string_view>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/RequestOptions.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::http
{

using FutureHttpResponse = std::future<std::expected<HttpResponse, sihd::util::Error>>;

std::expected<HttpResponse, sihd::util::Error> get(std::string_view url,
                                                   const RequestOptions & options = RequestOptions::none());

std::expected<HttpResponse, sihd::util::Error> post(std::string_view url,
                                                    sihd::util::ArrCharView data_view,
                                                    const RequestOptions & options = RequestOptions::none());

std::expected<HttpResponse, sihd::util::Error>
    put(std::string_view url, std::string_view file_path, const RequestOptions & options = RequestOptions::none());

std::expected<HttpResponse, sihd::util::Error> del(std::string_view url,
                                                   const RequestOptions & options = RequestOptions::none());

std::expected<HttpResponse, sihd::util::Error> options(std::string_view url,
                                                       const RequestOptions & options = RequestOptions::none());

std::expected<HttpResponse, sihd::util::Error> patch(std::string_view url,
                                                     sihd::util::ArrCharView data_view,
                                                     const RequestOptions & options = RequestOptions::none());

std::expected<HttpResponse, sihd::util::Error> head(std::string_view url,
                                                    const RequestOptions & options = RequestOptions::none());

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_get(std::string_view url, const RequestOptions & options = RequestOptions::none());

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_post(std::string_view url,
               sihd::util::ArrCharView data_view,
               const RequestOptions & options = RequestOptions::none());

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_put(std::string_view url,
              std::string_view file_path,
              const RequestOptions & options = RequestOptions::none());

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_del(std::string_view url, const RequestOptions & options = RequestOptions::none());

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_options(std::string_view url, const RequestOptions & options = RequestOptions::none());

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_patch(std::string_view url,
                sihd::util::ArrCharView data_view,
                const RequestOptions & options = RequestOptions::none());

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_head(std::string_view url, const RequestOptions & options = RequestOptions::none());

} // namespace sihd::http

#endif
