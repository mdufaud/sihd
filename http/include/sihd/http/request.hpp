#ifndef __SIHD_HTTP_REQUEST_HPP__
#define __SIHD_HTTP_REQUEST_HPP__

#include <sihd/http/CurlOptions.hpp>
#include <sihd/http/HttpRequest.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/util/ArrayView.hpp>

namespace sihd::http
{

/**
 * For async you can include <future> then
 * auto future = std::async(std::launch::async, sihd::http:req::get, url, options);
 * ...
 * future.wait();
 * auto response = future.get();
 */

std::optional<HttpResponse> get(std::string_view url, const CurlOptions & options = CurlOptions::none());

std::optional<HttpResponse>
    post(std::string_view url, sihd::util::ArrCharView data_view, const CurlOptions & options = CurlOptions::none());

std::optional<HttpResponse>
    put(std::string_view url, std::string_view file_path, const CurlOptions & options = CurlOptions::none());

std::optional<HttpResponse> del(std::string_view url, const CurlOptions & options = CurlOptions::none());

} // namespace sihd::http

#endif
