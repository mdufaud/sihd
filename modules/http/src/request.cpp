#include <future>

#include <sihd/http/request.hpp>
#include <sihd/util/Logger.hpp>

#include "Client.hpp"

namespace sihd::http
{

SIHD_LOGGER;

namespace
{

// a response cut short by max_response_size is still a response: keep the
// truncated content instead of reporting a transport failure
template <typename Send>
std::optional<HttpResponse> one_shot(Send && send)
{
    HttpResponse response;
    auto result = send(response);
    if (!result)
    {
        SIHD_LOG(error, "request: {}", result.error().message);
        return std::nullopt;
    }
    return response;
}

} // namespace

// one-shot helpers: each call opens its own connection, use a Navigator to reuse it

std::optional<HttpResponse> get(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Get, options, response); });
}

std::optional<HttpResponse>
    post(std::string_view url, sihd::util::ArrCharView data_view, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send(url, HttpRequest::Post, data_view, options, response); });
}

std::optional<HttpResponse> put(std::string_view url, std::string_view file_path, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send_file(url, file_path, HttpRequest::Put, options, response); });
}

std::optional<HttpResponse> del(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Delete, options, response); });
}

std::optional<HttpResponse> options(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Options, options, response); });
}

std::optional<HttpResponse>
    patch(std::string_view url, sihd::util::ArrCharView data_view, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send(url, HttpRequest::Patch, data_view, options, response); });
}

std::optional<HttpResponse> head(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Head, options, response); });
}

// the url and the body are copied: the future may outlive a caller's temporary

std::future<std::optional<HttpResponse>> async_get(std::string_view url, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), options] { return get(url, options); });
}

std::future<std::optional<HttpResponse>>
    async_post(std::string_view url, sihd::util::ArrCharView data_view, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), data = data_view.cpp_str(), options] {
        return post(url, data, options);
    });
}

std::future<std::optional<HttpResponse>>
    async_put(std::string_view url, std::string_view file_path, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), file_path = std::string(file_path), options] {
        return put(url, file_path, options);
    });
}

std::future<std::optional<HttpResponse>> async_del(std::string_view url, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), options] { return del(url, options); });
}

std::future<std::optional<HttpResponse>> async_options(std::string_view url, const RequestOptions & req_options)
{
    return std::async(std::launch::async, [url = std::string(url), req_options] { return options(url, req_options); });
}

std::future<std::optional<HttpResponse>>
    async_patch(std::string_view url, sihd::util::ArrCharView data_view, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), data = data_view.cpp_str(), options] {
        return patch(url, data, options);
    });
}

std::future<std::optional<HttpResponse>> async_head(std::string_view url, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), options] { return head(url, options); });
}

} // namespace sihd::http
