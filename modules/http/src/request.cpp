#include <stdexcept>

#include "curl/CurlRequest.hpp"
#include <sihd/http/request.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::http
{

SIHD_LOGGER;

using namespace sihd::http::curl;

std::optional<HttpResponse> get(std::string_view url, const CurlOptions & options)
{
    CurlRequest curl;

    try
    {
        curl.init();
        curl.set(CURLOPT_HTTPGET, 1L);
        return curl.send_request(url, options);
    }
    catch (const std::runtime_error & error)
    {
        SIHD_LOG(error, "GET request {}", error.what());
    }

    return std::nullopt;
}

std::optional<HttpResponse>
    post(std::string_view url, sihd::util::ArrCharView data_view, const CurlOptions & options)
{
    CurlRequest curl;

    try
    {
        curl.init();
        curl.set(CURLOPT_POST, 1L);

        if (data_view.empty() == false)
            curl.set(CURLOPT_POSTFIELDS, data_view.data());

        if (options.form_parameters.empty() == false)
            curl.add_form(options.form_parameters);

        if (options.file.has_value())
        {
            if (options.file->data.empty() == false)
                curl.add_file_to_form(options.file->form_name, options.file->file_name, options.file->data);
            else if (options.file->file_path.empty() == false)
                curl.add_filestream_to_form(options.file->form_name,
                                            options.file->file_name,
                                            options.file->file_path);
        }

        return curl.send_request(url, options);
    }
    catch (const std::runtime_error & error)
    {
        SIHD_LOG(error, "POST request {}", error.what());
    }

    return std::nullopt;
}

std::optional<HttpResponse> put(std::string_view url, std::string_view file_path, const CurlOptions & options)
{
    CurlRequest curl;

    try
    {
        auto content_opt = sihd::sys::fs::read_all(file_path);
        if (!content_opt.has_value())
            throw std::runtime_error(fmt::format("cannot read file: {}", file_path));

        const std::string & data = content_opt.value();

        curl.init();
        curl.set(CURLOPT_CUSTOMREQUEST, "PUT");
        curl.set(CURLOPT_POSTFIELDS, data.data());
        curl.set(CURLOPT_POSTFIELDSIZE, (long)data.size());
        auto resp = curl.send_request(url, options);

        return resp;
    }
    catch (const std::runtime_error & error)
    {
        SIHD_LOG(error, "PUT request {}", error.what());
    }

    return std::nullopt;
}

std::optional<HttpResponse> del(std::string_view url, const CurlOptions & options)
{
    CurlRequest curl;

    try
    {
        curl.init();
        curl.set(CURLOPT_CUSTOMREQUEST, "DELETE");
        return curl.send_request(url, options);
    }
    catch (const std::runtime_error & error)
    {
        SIHD_LOG(error, "DELETE request {}", error.what());
    }

    return std::nullopt;
}

std::optional<HttpResponse> options(std::string_view url, const CurlOptions & options)
{
    CurlRequest curl;

    try
    {
        curl.init();
        curl.set(CURLOPT_CUSTOMREQUEST, "OPTIONS");
        curl.set(CURLOPT_NOBODY, 1L);
        return curl.send_request(url, options);
    }
    catch (const std::runtime_error & error)
    {
        SIHD_LOG(error, "OPTIONS request {}", error.what());
    }

    return std::nullopt;
}

std::optional<HttpResponse>
    patch(std::string_view url, sihd::util::ArrCharView data_view, const CurlOptions & options)
{
    CurlRequest curl;

    try
    {
        curl.init();
        curl.set(CURLOPT_CUSTOMREQUEST, "PATCH");

        if (data_view.empty() == false)
        {
            curl.set(CURLOPT_POSTFIELDS, data_view.data());
            curl.set(CURLOPT_POSTFIELDSIZE, (long)data_view.size());
        }

        return curl.send_request(url, options);
    }
    catch (const std::runtime_error & error)
    {
        SIHD_LOG(error, "PATCH request {}", error.what());
    }

    return std::nullopt;
}

std::optional<HttpResponse> head(std::string_view url, const CurlOptions & options)
{
    CurlRequest curl;

    try
    {
        curl.init();
        curl.set(CURLOPT_CUSTOMREQUEST, "HEAD");
        curl.set(CURLOPT_NOBODY, 1L);
        return curl.send_request(url, options);
    }
    catch (const std::runtime_error & error)
    {
        SIHD_LOG(error, "HEAD request {}", error.what());
    }

    return std::nullopt;
}

std::future<std::optional<HttpResponse>> async_get(std::string_view url, const CurlOptions & curlopt)
{
    return std::async(std::launch::async, get, url, curlopt);
}

std::future<std::optional<HttpResponse>>
    async_post(std::string_view url, sihd::util::ArrCharView data_view, const CurlOptions & curlopt)
{
    return std::async(std::launch::async, post, url, data_view, curlopt);
}

std::future<std::optional<HttpResponse>>
    async_put(std::string_view url, std::string_view file_path, const CurlOptions & curlopt)
{
    return std::async(std::launch::async, put, url, file_path, curlopt);
}

std::future<std::optional<HttpResponse>> async_del(std::string_view url, const CurlOptions & curlopt)
{
    return std::async(std::launch::async, del, url, curlopt);
}

std::future<std::optional<HttpResponse>> async_options(std::string_view url, const CurlOptions & curlopt)
{
    return std::async(std::launch::async, options, url, curlopt);
}

std::future<std::optional<HttpResponse>>
    async_patch(std::string_view url, sihd::util::ArrCharView data_view, const CurlOptions & curlopt)
{
    return std::async(std::launch::async, patch, url, data_view, curlopt);
}

std::future<std::optional<HttpResponse>> async_head(std::string_view url, const CurlOptions & curlopt)
{
    return std::async(std::launch::async, head, url, curlopt);
}

} // namespace sihd::http
