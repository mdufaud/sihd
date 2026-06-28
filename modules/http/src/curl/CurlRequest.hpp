#ifndef __SIHD_HTTP_SRC_CURL_CURLREQUEST_HPP__
#define __SIHD_HTTP_SRC_CURL_CURLREQUEST_HPP__

#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/core.h>

#include "utils.hpp"
#include <sihd/http/CurlOptions.hpp>
#include <sihd/http/HttpHeader.hpp>
#include <sihd/http/HttpResponse.hpp>
#include <sihd/util/Url.hpp>

namespace sihd::http::curl
{

using sihd::util::Url;

struct ProgressCallbackWrapper
{
        std::function<bool(curl_off_t, curl_off_t, curl_off_t, curl_off_t)> method = {};
};

size_t curl_content_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
size_t curl_header_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
int curl_progress_callback(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

struct CurlRequest
{
        CURL *handle = nullptr;
        curl_slist *header = nullptr;
        curl_mime *mime = nullptr;
        ProgressCallbackWrapper progress_callback_wrapper;

        CurlRequest() = default;
        ~CurlRequest()
        {
            if (handle != nullptr)
                curl_easy_cleanup(handle);
            if (header != nullptr)
                curl_slist_free_all(header);
            if (mime != nullptr)
                curl_mime_free(mime);
        }

        CurlRequest(const CurlRequest &) = delete;
        CurlRequest & operator=(const CurlRequest &) = delete;

        bool init()
        {
            curl_init_once();
            handle = curl_easy_init();
            if (handle == nullptr)
                throw std::runtime_error("could not initialize curl");
            return handle != nullptr;
        }

        template <typename T>
        void get_info(CURLINFO opt, T *data)
        {
            CURLcode code = curl_easy_getinfo(handle, opt, data);
            if (code != CURLE_OK)
                throw std::runtime_error(
                    fmt::format("could not get curl option: {}", curl_easy_strerror(code)));
        }

        template <typename T>
        void set(CURLoption opt, const T & data)
        {
            CURLcode code = curl_easy_setopt(handle, opt, data);
            if (code != CURLE_OK)
                throw std::runtime_error(
                    fmt::format("could not set curl option: {}", curl_easy_strerror(code)));
        }

        void perform_request()
        {
            CURLcode code = curl_easy_perform(handle);
            if (code != CURLE_OK)
                throw std::runtime_error(
                    fmt::format("could not perform request: {}", curl_easy_strerror(code)));
        }

        void append_header(std::string_view str)
        {
            curl_slist *new_header = curl_slist_append(header, str.data());
            if (new_header == nullptr)
                throw std::runtime_error(fmt::format("could add header '{}'", str));
            header = new_header;
        }

        std::string create_url_with_parameters(std::string_view url,
                                               const std::map<std::string, std::string> & parameters)
        {
            if (parameters.empty())
                return std::string(url);
            Url parsed(url);
            for (const auto & [key, val] : parameters)
                parsed.query_params[key] = val;
            return parsed.encode();
        }

        void add_filestream_to_form(const std::string & form_name,
                                    const std::string & file_name,
                                    const std::string & file_path)
        {
            if (mime == nullptr)
                mime = curl_mime_init(handle);
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_name(part, form_name.c_str());
            curl_mime_filedata(part, file_path.c_str());
            if (file_name.empty() == false)
                curl_mime_filename(part, file_name.c_str());
        }

        void add_file_to_form(const std::string & form_name,
                              const std::string & file_name,
                              const std::vector<uint8_t> & data)
        {
            if (mime == nullptr)
                mime = curl_mime_init(handle);
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_name(part, form_name.c_str());
            curl_mime_filename(part, file_name.c_str());
            curl_mime_data(part, (const char *)data.data(), data.size());
        }

        void add_form(const std::map<std::string, std::string> & form_parameters)
        {
            if (mime == nullptr)
                mime = curl_mime_init(handle);
            for (const auto & [name, data] : form_parameters)
            {
                curl_mimepart *part = curl_mime_addpart(mime);
                curl_mime_name(part, name.c_str());
                curl_mime_data(part, data.c_str(), data.size());
            }
        }

        HttpResponse send_request(std::string_view url, const CurlOptions & options);
};

} // namespace sihd::http::curl

#endif
