#include "CurlRequest.hpp"

#include <sihd/util/tools.hpp>

namespace sihd::http::curl
{

size_t curl_content_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    std::string *content_str = static_cast<std::string *>(userdata);
    content_str->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t curl_header_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    HttpHeader *header = static_cast<HttpHeader *>(userdata);
    std::string_view header_str(ptr, size * nmemb);
    header->add_header_from_str(header_str);
    return size * nmemb;
}

int curl_progress_callback(void *userdata,
                           curl_off_t dltotal,
                           curl_off_t dlnow,
                           curl_off_t ultotal,
                           curl_off_t ulnow)
{
    ProgressCallbackWrapper *wrapper = static_cast<ProgressCallbackWrapper *>(userdata);
    if (wrapper->method(dltotal, dlnow, ultotal, ulnow))
        return CURL_PROGRESSFUNC_CONTINUE;
    return 1;
}

HttpResponse CurlRequest::send_request(std::string_view url, const CurlOptions & options)
{
    if (sihd::util::tools::maximum_one_true(!options.username.empty() && !options.password.empty(),
                                            !options.token.empty())
        == false)
        throw std::runtime_error("there can only be one authentication method");

    this->set(CURLOPT_VERBOSE, options.verbose);

    const std::string url_with_parameters = this->create_url_with_parameters(url, options.parameters);
    this->set(CURLOPT_URL, url_with_parameters.c_str());

    // Read HttpHeaders
    HttpResponse response;
    this->set(CURLOPT_HEADERFUNCTION, curl_header_callback);
    this->set(CURLOPT_HEADERDATA, static_cast<void *>(&response.http_header()));

    // Read body
    std::string content;
    this->set(CURLOPT_WRITEFUNCTION, curl_content_callback);
    this->set(CURLOPT_WRITEDATA, static_cast<void *>(&content));

    if (options.upload_progress)
    {
        this->set(CURLOPT_NOPROGRESS, 0);
        this->set(CURLOPT_XFERINFOFUNCTION, curl_progress_callback);
        progress_callback_wrapper.method = options.upload_progress;
        this->set(CURLOPT_XFERINFODATA, static_cast<void *>(&progress_callback_wrapper));
    }

    // Timeout
    this->set(CURLOPT_TIMEOUT, options.timeout_s);
    this->set(CURLOPT_CONNECTTIMEOUT, options.connect_timeout_s);

    // Follow location
    this->set(CURLOPT_FOLLOWLOCATION, (long)options.follow_location);

    // User agent
    if (!options.user_agent.empty())
        this->set(CURLOPT_USERAGENT, options.user_agent.c_str());

    // SSL
    this->set(CURLOPT_SSL_VERIFYPEER, (int)options.ssl_verify_peer);
    this->set(CURLOPT_SSL_VERIFYHOST, (int)options.ssl_verify_host);

    // Basic Auth
    if (!options.username.empty() && !options.password.empty())
    {
        this->set(CURLOPT_USERNAME, options.username.c_str());
        this->set(CURLOPT_PASSWORD, options.password.c_str());
    }

    // Token Auth
    if (!options.token.empty())
    {
        const std::string header_str = fmt::format("Authorization: Bearer {}", options.token);
        this->append_header(header_str);
    }

    // Proxy ("" disables env proxy, non-empty sets it)
    if (options.proxy.has_value())
    {
        this->set(CURLOPT_PROXY, options.proxy->c_str());
        if (!options.proxy_username.empty())
        {
            this->set(CURLOPT_PROXYUSERNAME, options.proxy_username.c_str());
            this->set(CURLOPT_PROXYPASSWORD, options.proxy_password.c_str());
        }
    }

    // Headers
    for (const auto & [header_name, header_value] : options.headers)
    {
        const std::string header_str = fmt::format("{}: {}", header_name, header_value);
        this->append_header(header_str);
    }
    this->set(CURLOPT_HTTPHEADER, header);

    // Add form
    if (mime != nullptr)
        this->set(CURLOPT_MIMEPOST, mime);

    this->perform_request();

    // Create response

    int http_status = 0;
    this->get_info(CURLINFO_RESPONSE_CODE, &http_status);

    char *content_type = nullptr;
    this->get_info(CURLINFO_CONTENT_TYPE, &content_type);

    response.set_status(http_status);
    if (content_type != nullptr)
        response.set_content_type(content_type);
    response.set_content(content);

    return response;
}

} // namespace sihd::http::curl
