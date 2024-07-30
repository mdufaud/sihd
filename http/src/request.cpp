#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <sihd/http/request.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/tools.hpp>

namespace sihd::http
{

SIHD_LOGGER;

namespace
{

struct CurlHandler
{
        CurlHandler()
        {
            const CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
            if (code != CURLE_OK)
            {
                throw std::runtime_error(fmt::format("Curl initialization failed: {}", curl_easy_strerror(code)));
            }
        }

        ~CurlHandler() { curl_global_cleanup(); }
};

static std::mutex g_curl_mutex;
static std::unique_ptr<CurlHandler> g_curl_handler;

void init_first_curl()
{
    std::lock_guard l(g_curl_mutex);
    if (g_curl_handler.get() == nullptr)
        g_curl_handler = std::make_unique<CurlHandler>();
}

size_t curl_content_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    std::string *content_str = static_cast<std::string *>(userdata);
    content_str->append(ptr, size * nmemb);
    return size * nmemb;
}

// size_t curl_put_read_callback(char *ptr, size_t size, size_t nmemb, void *stream)
// {
//     /* in real-world cases, this would probably get this data differently
//        as this fread() stuff is exactly what the library already would do
//        by default internally */
//     return fread(ptr, size, nmemb, (FILE *)stream);
// }

size_t curl_header_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    HttpHeader *header = static_cast<HttpHeader *>(userdata);
    std::string_view header_str(ptr, size * nmemb);
    header->add_header_from_str(header_str);
    return size * nmemb;
}

struct ProgressCallbackWrapper
{
        std::function<bool(curl_off_t, curl_off_t, curl_off_t, curl_off_t)> method = {};
};

int curl_progress_callback(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    ProgressCallbackWrapper *wrapper = static_cast<ProgressCallbackWrapper *>(userdata);
    if (wrapper->method(dltotal, dlnow, ultotal, ulnow))
        return CURL_PROGRESSFUNC_CONTINUE;
    return 1;
}

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

        bool init()
        {
            init_first_curl();
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
                throw std::runtime_error(fmt::format("could not get curl option: {}", curl_easy_strerror(code)));
        }

        template <typename T>
        void set(CURLoption opt, const T & data)
        {
            CURLcode code = curl_easy_setopt(handle, opt, data);
            if (code != CURLE_OK)
                throw std::runtime_error(fmt::format("could not set curl option: {}", curl_easy_strerror(code)));
        }

        void perform_request()
        {
            CURLcode code = curl_easy_perform(handle);
            if (code != CURLE_OK)
                throw std::runtime_error(fmt::format("could not perform request: {}", curl_easy_strerror(code)));
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
            std::string final_url(url);
            uint i = 0U;
            for (const auto & [key, val] : parameters)
            {
                final_url = fmt::format("{}{}{}={}", final_url, i++ == 0 ? "?" : "&", key, val);
            }
            return final_url;
        }

        void add_filestream_to_form(const std::string & form_name,
                                    const std::string & file_name,
                                    const std::string & file_path)
        {
            if (mime == nullptr)
                mime = curl_mime_init(handle);
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_name(part, form_name.c_str());
            // will read the file while transfering
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

        HttpResponse send_request(std::string_view url, const CurlOptions & options)
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
};

} // namespace

using namespace sihd::util;

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

std::optional<HttpResponse> post(std::string_view url, sihd::util::ArrCharView data_view, const CurlOptions & options)
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
                curl.add_filestream_to_form(options.file->form_name, options.file->file_name, options.file->file_path);
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
        File file(file_path, "rb");
        if (file.is_open() == false)
            throw std::runtime_error(fmt::format("cannot open file: {}", file_path));

        curl.init();
        curl.set(CURLOPT_UPLOAD, 1L);
        // curl.set(CURLOPT_READFUNCTION, curl_put_read_callback);
        curl.set(CURLOPT_READDATA, static_cast<void *>(file.file()));
        curl.set(CURLOPT_INFILESIZE_LARGE, (curl_off_t)file.file_size());
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

} // namespace sihd::http
