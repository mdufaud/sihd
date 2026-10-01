#include <expected>
#include <future>

#include <sihd/http/request.hpp>

#include "Client.hpp"

namespace sihd::http
{

namespace
{

template <typename Send>
std::expected<HttpResponse, sihd::util::Error> one_shot(Send && send)
{
    HttpResponse response;
    auto result = send(response);
    SIHD_UNEXPECTED_RETURN(result);
    return response;
}

} // namespace

// one-shot helpers: each call opens its own connection, use a Navigator to reuse it

std::expected<HttpResponse, sihd::util::Error> get(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Get, options, response); });
}

std::expected<HttpResponse, sihd::util::Error>
    post(std::string_view url, sihd::util::ArrCharView data_view, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send(url, HttpRequest::Post, data_view, options, response); });
}

std::expected<HttpResponse, sihd::util::Error>
    put(std::string_view url, std::string_view file_path, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send_file(url, file_path, HttpRequest::Put, options, response); });
}

std::expected<HttpResponse, sihd::util::Error> del(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Delete, options, response); });
}

std::expected<HttpResponse, sihd::util::Error> options(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Options, options, response); });
}

std::expected<HttpResponse, sihd::util::Error>
    patch(std::string_view url, sihd::util::ArrCharView data_view, const RequestOptions & options)
{
    Client client;
    return one_shot(
        [&](HttpResponse & response) { return client.send(url, HttpRequest::Patch, data_view, options, response); });
}

std::expected<HttpResponse, sihd::util::Error> head(std::string_view url, const RequestOptions & options)
{
    Client client;
    return one_shot([&](HttpResponse & response) { return client.send(url, HttpRequest::Head, options, response); });
}

// the url and the body are copied: the future may outlive a caller's temporary

std::future<std::expected<HttpResponse, sihd::util::Error>> async_get(std::string_view url,
                                                                      const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), options] { return get(url, options); });
}

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_post(std::string_view url, sihd::util::ArrCharView data_view, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), data = data_view.cpp_str(), options] {
        return post(url, data, options);
    });
}

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_put(std::string_view url, std::string_view file_path, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), file_path = std::string(file_path), options] {
        return put(url, file_path, options);
    });
}

std::future<std::expected<HttpResponse, sihd::util::Error>> async_del(std::string_view url,
                                                                      const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), options] { return del(url, options); });
}

std::future<std::expected<HttpResponse, sihd::util::Error>> async_options(std::string_view url,
                                                                          const RequestOptions & req_options)
{
    return std::async(std::launch::async, [url = std::string(url), req_options] { return options(url, req_options); });
}

std::future<std::expected<HttpResponse, sihd::util::Error>>
    async_patch(std::string_view url, sihd::util::ArrCharView data_view, const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), data = data_view.cpp_str(), options] {
        return patch(url, data, options);
    });
}

std::future<std::expected<HttpResponse, sihd::util::Error>> async_head(std::string_view url,
                                                                       const RequestOptions & options)
{
    return std::async(std::launch::async, [url = std::string(url), options] { return head(url, options); });
}

} // namespace sihd::http
