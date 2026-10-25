#include <expected>
#include <future>

#include <sihd/http/request.hpp>
#include <sihd/util/Result.hpp>

#include "Client.hpp"

using namespace sihd::util;

namespace sihd::http
{

namespace
{

template <typename Send>
Result<HttpResponse> one_shot(Send && send)
{
    HttpResponse response;
    auto result = send(response);
    SIHD_UNEXPECTED_RETURN(result);
    return response;
}

} // namespace

// one-shot helpers: each call opens its own connection, use a Navigator to reuse it

Result<HttpResponse> get(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Get, options, response); });
}

Result<HttpResponse> post(std::string_view url, ArrCharView data_view, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send(url, HttpRequest::Post, data_view, options, response); });
}

Result<HttpResponse> put(std::string_view url, const std::string & file_path, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send_file(url, file_path, HttpRequest::Put, options, response); });
}

Result<HttpResponse> del(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Delete, options, response); });
}

Result<HttpResponse> options(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Options, options, response); });
}

Result<HttpResponse> patch(std::string_view url, ArrCharView data_view, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send(url, HttpRequest::Patch, data_view, options, response); });
}

Result<HttpResponse> head(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Head, options, response); });
}

// the url and the body are copied: the future may outlive a caller's temporary

std::future<Result<HttpResponse>> async_get(const std::string & url, const RequestOptions & options)
{
    return std::async(std::launch::async, [url, options] { return get(url, options); });
}

std::future<Result<HttpResponse>>
    async_post(const std::string & url, ArrCharView data_view, const RequestOptions & options)
{
    return std::async(std::launch::async,
                      [url, data = data_view.cpp_str(), options] { return post(url, data, options); });
}

std::future<Result<HttpResponse>>
    async_put(const std::string & url, const std::string & file_path, const RequestOptions & options)
{
    return std::async(std::launch::async, [url, file_path, options] { return put(url, file_path, options); });
}

std::future<Result<HttpResponse>> async_del(const std::string & url, const RequestOptions & options)
{
    return std::async(std::launch::async, [url, options] { return del(url, options); });
}

std::future<Result<HttpResponse>> async_options(const std::string & url, const RequestOptions & req_options)
{
    return std::async(std::launch::async, [url, req_options] { return options(url, req_options); });
}

std::future<Result<HttpResponse>>
    async_patch(const std::string & url, ArrCharView data_view, const RequestOptions & options)
{
    return std::async(std::launch::async,
                      [url, data = data_view.cpp_str(), options] { return patch(url, data, options); });
}

std::future<Result<HttpResponse>> async_head(const std::string & url, const RequestOptions & options)
{
    return std::async(std::launch::async, [url, options] { return head(url, options); });
}

} // namespace sihd::http
