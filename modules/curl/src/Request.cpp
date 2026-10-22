#include <algorithm>
#include <expected>
#include <mutex>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <sihd/curl/HeaderList.hpp>
#include <sihd/curl/Mime.hpp>
#include <sihd/curl/Request.hpp>
#include <sihd/util/AtExit.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Result.hpp>

#include "impl.hpp"

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::util
{
template <>
struct ErrorResolver<CURLcode>
{
        static ErrorCode code(CURLcode native)
        {
            switch (native)
            {
                case CURLE_OPERATION_TIMEDOUT:
                    return ErrorCode::timeout;
                case CURLE_COULDNT_RESOLVE_HOST:
                case CURLE_COULDNT_RESOLVE_PROXY:
                    return ErrorCode::not_found;
                case CURLE_COULDNT_CONNECT:
                case CURLE_RECV_ERROR:
                case CURLE_SEND_ERROR:
                    return ErrorCode::io_error;
                case CURLE_REMOTE_ACCESS_DENIED:
                    return ErrorCode::permission_denied;
                case CURLE_UNSUPPORTED_PROTOCOL:
                    return ErrorCode::not_supported;
                case CURLE_URL_MALFORMAT:
                    return ErrorCode::invalid_argument;
                case CURLE_ABORTED_BY_CALLBACK:
                    return ErrorCode::interrupted;
                default:
                    return ErrorCode::io_error;
            }
        }
};
} // namespace sihd::util

namespace sihd::curl
{

namespace
{

struct GlobalCleanup: public IRunnable
{
        bool run() override
        {
            curl_global_cleanup();
            return true;
        }
};

std::unexpected<Error> err(ErrorCode code, std::string message)
{
    return std::unexpected(Error(code, std::move(message)));
}

template <typename... Args>
    requires(sizeof...(Args) != 0)
std::unexpected<Error> err(ErrorCode code, fmt::format_string<Args...> format, Args &&...args)
{
    return err(code, fmt::format(format, std::forward<Args>(args)...));
}

// curl_global_init() is not thread-safe and must run before any other curl call, from a single
// thread: funnel every request creation here. A failed init is not sticky, the next one retries
Result<void> global_init()
{
    static std::mutex mutex;
    static bool initialized = false;
    std::lock_guard lock(mutex);
    if (initialized)
        return {};
    CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
    if (code != CURLE_OK)
        return err(not_initialized, "curl_global_init failed: {}", curl_easy_strerror(code));
    // cleanup joins sihd's exit chain so it is ordered with the other
    // exit handlers instead of racing them in the C atexit chain
    if (util::atexit::install())
        util::atexit::add_handler(new GlobalCleanup());
    else
        std::atexit(curl_global_cleanup);
    initialized = true;
    return {};
}

template <typename Impl, typename T>
Result<void> set_opt(Impl & impl, CURLoption option, const T & value)
{
    if (impl.curl == nullptr)
        return err(not_initialized, impl.init_error.empty() ? "no curl handle" : impl.init_error);
    CURLcode code = curl_easy_setopt(impl.curl, option, value);
    if (code != CURLE_OK)
    {
        impl.code = code;
        return err(error_from(code), "could not set curl option: {}", curl_easy_strerror(code));
    }
    return {};
}

template <typename T>
bool get_info(CURL *curl, CURLINFO info, T & value)
{
    if (curl == nullptr)
        return false;
    return curl_easy_getinfo(curl, info, &value) == CURLE_OK;
}

struct Callbacks
{
        Request::WriteFn write;
        Request::WriteFn header;
        Request::ReadFn read;
        Request::ProgressFn progress;
};

template <Request::WriteFn Callbacks::*member>
size_t data_trampoline(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *callbacks = static_cast<Callbacks *>(userdata);
    const Request::WriteFn & fn = callbacks->*member;
    if (!fn)
        return size * nmemb;
    ArrByteView view((const int8_t *)ptr, size * nmemb);
    return fn(view) ? size * nmemb : 0;
}

size_t read_trampoline(char *buffer, size_t size, size_t nmemb, void *userdata)
{
    static_assert(Request::READ_ABORT == CURL_READFUNC_ABORT);
    auto *callbacks = static_cast<Callbacks *>(userdata);
    if (!callbacks->read)
        return 0;
    return callbacks->read(buffer, size * nmemb);
}

int progress_trampoline(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    auto *callbacks = static_cast<Callbacks *>(userdata);
    if (!callbacks->progress)
        return CURL_PROGRESSFUNC_CONTINUE;
    XferProgress prog {(long)dltotal, (long)dlnow, (long)ultotal, (long)ulnow};
    return callbacks->progress(prog) ? CURL_PROGRESSFUNC_CONTINUE : 1;
}

Result<void> mime_check(CURLcode code, std::string_view what)
{
    if (code == CURLE_OK)
        return {};
    return err(error_from(code), "could not {}: {}", what, curl_easy_strerror(code));
}

Result<void> add_mime_part(curl_mime *handle, const MimePart & part)
{
    curl_mimepart *mime_part = curl_mime_addpart(handle);
    if (mime_part == nullptr)
        return err(out_of_memory, "could not add mime part");

    if (!part.name.empty())
    {
        auto named = mime_check(curl_mime_name(mime_part, part.name.c_str()), "set part name");
        SIHD_UNEXPECTED_RETURN(named);
    }

    if (!part.data.empty())
    {
        auto filled = mime_check(curl_mime_data(mime_part, (const char *)part.data.data(), part.data.size()),
                                 "set part data");
        SIHD_UNEXPECTED_RETURN(filled);
    }

    if (!part.path.empty())
    {
        auto filed = mime_check(curl_mime_filedata(mime_part, part.path.c_str()), "set part file");
        SIHD_UNEXPECTED_RETURN(filed);
    }

    if (!part.filename.empty())
    {
        auto filenamed = mime_check(curl_mime_filename(mime_part, part.filename.c_str()), "set part filename");
        SIHD_UNEXPECTED_RETURN(filenamed);
    }

    if (!part.content_type.empty())
        return mime_check(curl_mime_type(mime_part, part.content_type.c_str()), "set part content type");
    return {};
}

} // namespace

struct SlistFree
{
        void operator()(curl_slist *list) const { curl_slist_free_all(list); }
};

struct Request::Impl
{
        CURL *curl = nullptr;
        CURLcode code = CURLE_OK;
        // non-empty: construction failed and left the handle defunct
        std::string init_error;

        std::unique_ptr<curl_slist, SlistFree> header_list;
        std::unique_ptr<curl_slist, SlistFree> resolve_list;
        curl_mime *mime = nullptr;

        std::string url;
        std::string user_agent;
        std::string user;
        std::string password;
        std::string proxy_url;
        std::string proxy_user;
        std::string proxy_password;
        std::string custom_request;
        std::string accept_encoding;
        std::string cookie_jar;
        std::string cookies_file;
        std::string cookie_line;

        Callbacks callbacks;

        ~Impl()
        {
            if (mime != nullptr)
                curl_mime_free(mime);
            if (curl != nullptr)
                curl_easy_cleanup(curl);
        }
};

Request::Request(): _impl(std::make_unique<Impl>())
{
    if (auto initialized = global_init(); !initialized)
    {
        _impl->init_error = std::move(initialized.error()).message;
        return;
    }
    _impl->curl = curl_easy_init();
    if (_impl->curl == nullptr)
        _impl->init_error = "could not initialize curl request";
}

Request::~Request() = default;

Request::Request(Request && other): _impl(std::move(other._impl))
{
    other._impl = std::make_unique<Impl>();
}

Request & Request::operator=(Request && other)
{
    if (this != &other)
    {
        _impl = std::move(other._impl);
        other._impl = std::make_unique<Impl>();
    }
    return *this;
}

Result<void> Request::set_url(std::string_view url)
{
    _impl->url = std::string(url);
    return set_opt(*_impl, CURLOPT_URL, _impl->url.c_str());
}

Result<void> Request::set_verbose(bool on)
{
    return set_opt(*_impl, CURLOPT_VERBOSE, (long)on);
}

Result<void> Request::set_follow_location(bool on)
{
    return set_opt(*_impl, CURLOPT_FOLLOWLOCATION, (long)on);
}

Result<void> Request::set_resolve(std::string_view host, int port, const std::vector<std::string> & addresses)
{
    std::unique_ptr<curl_slist, SlistFree> list;
    if (!addresses.empty())
    {
        constexpr std::string_view host_forbidden(":,\0", 3);
        constexpr std::string_view address_forbidden(",\0", 2);

        const bool valid_host = !host.empty() && host.find_first_of(host_forbidden) == std::string_view::npos;
        const bool valid_port = port > 0 && port <= 65535;
        const bool valid_addresses = std::ranges::all_of(addresses, [&](const std::string & address) {
            return !address.empty() && address.find_first_of(address_forbidden) == std::string::npos;
        });
        if (!valid_host || !valid_port || !valid_addresses)
            return std::unexpected(Error(invalid_argument, "invalid resolve entry for '{}'", host));

        std::string entry = fmt::format("{}:{}:{}", host, port, fmt::join(addresses, ","));
        list.reset(curl_slist_append(nullptr, entry.c_str()));
        if (list == nullptr)
            return std::unexpected(Error(out_of_memory, "could not allocate the resolve entry"));
    }

    auto resolved = set_opt(*_impl, CURLOPT_RESOLVE, list.get());
    SIHD_UNEXPECTED_RETURN(resolved);
    _impl->resolve_list = std::move(list);
    return {};
}

Result<void> Request::set_timeout(Duration duration)
{
    return set_opt(*_impl, CURLOPT_TIMEOUT_MS, (long)duration.milliseconds());
}

Result<void> Request::set_connect_timeout(Duration duration)
{
    return set_opt(*_impl, CURLOPT_CONNECTTIMEOUT_MS, (long)duration.milliseconds());
}

Result<void> Request::set_user_agent(std::string_view user_agent)
{
    if (user_agent.empty())
        // an empty agent means no header: only a null value clears the option
        return set_opt(*_impl, CURLOPT_USERAGENT, (const char *)nullptr);
    _impl->user_agent = std::string(user_agent);
    return set_opt(*_impl, CURLOPT_USERAGENT, _impl->user_agent.c_str());
}

Result<void> Request::set_ssl_verify(bool verify_peer, bool verify_host)
{
    auto peer_set = set_opt(*_impl, CURLOPT_SSL_VERIFYPEER, (long)verify_peer);
    SIHD_UNEXPECTED_RETURN(peer_set);
    return set_opt(*_impl, CURLOPT_SSL_VERIFYHOST, verify_host ? 2L : 0L);
}

Result<void> Request::set_userpass(std::string_view user, std::string_view password)
{
    _impl->user = std::string(user);
    _impl->password = std::string(password);
    auto user_set = set_opt(*_impl, CURLOPT_USERNAME, _impl->user.c_str());
    SIHD_UNEXPECTED_RETURN(user_set);
    return set_opt(*_impl, CURLOPT_PASSWORD, _impl->password.c_str());
}

Result<void> Request::set_auth(Auth auth)
{
    long mask = CURLAUTH_ANY;
    if (auth == Auth::Basic)
        mask = CURLAUTH_BASIC;
    else if (auth == Auth::Digest)
        mask = CURLAUTH_DIGEST;
    return set_opt(*_impl, CURLOPT_HTTPAUTH, mask);
}

Result<void> Request::set_proxy(std::string_view url, Proxy type)
{
    _impl->proxy_url = std::string(url);

    long proxy_type = CURLPROXY_HTTP;
    if (type == Proxy::Socks4)
        proxy_type = CURLPROXY_SOCKS4;
    else if (type == Proxy::Socks5)
        proxy_type = CURLPROXY_SOCKS5;

    auto url_set = set_opt(*_impl, CURLOPT_PROXY, _impl->proxy_url.c_str());
    SIHD_UNEXPECTED_RETURN(url_set);
    return set_opt(*_impl, CURLOPT_PROXYTYPE, proxy_type);
}

Result<void> Request::set_proxy_auth(std::string_view user, std::string_view password)
{
    _impl->proxy_user = std::string(user);
    _impl->proxy_password = std::string(password);
    auto user_set = set_opt(*_impl, CURLOPT_PROXYUSERNAME, _impl->proxy_user.c_str());
    SIHD_UNEXPECTED_RETURN(user_set);
    return set_opt(*_impl, CURLOPT_PROXYPASSWORD, _impl->proxy_password.c_str());
}

Result<void> Request::clear_proxy()
{
    // an empty url disables the proxy: only a null value restores the environment's
    auto url_set = set_opt(*_impl, CURLOPT_PROXY, (const char *)nullptr);
    SIHD_UNEXPECTED_RETURN(url_set);
    return set_opt(*_impl, CURLOPT_PROXYTYPE, (long)CURLPROXY_HTTP);
}

Result<void> Request::set_headers(const HeaderList & headers)
{
    curl_slist *list = nullptr;
    for (const std::string & line : headers.lines())
    {
        // curl_slist_append takes a null-terminated string and copies it
        curl_slist *appended = curl_slist_append(list, line.c_str());
        if (appended == nullptr)
        {
            curl_slist_free_all(list);
            return err(out_of_memory, "could not build the header list");
        }
        list = appended;
    }
    _impl->header_list.reset(list);
    return set_opt(*_impl, CURLOPT_HTTPHEADER, list);
}

Result<void> Request::set_mime(const Mime & mime)
{
    if (_impl->curl == nullptr)
        return err(not_initialized, _impl->init_error.empty() ? "no curl handle" : _impl->init_error);
    curl_mime *handle = curl_mime_init(_impl->curl);
    if (handle == nullptr)
        return err(out_of_memory, "could not init mime");
    Defer free_mime([&handle] { curl_mime_free(handle); });
    for (const MimePart & part : mime.parts())
    {
        auto added = add_mime_part(handle, part);
        SIHD_UNEXPECTED_RETURN(added);
    }
    if (_impl->mime != nullptr)
        curl_mime_free(_impl->mime);
    _impl->mime = handle;
    free_mime.cancel();
    return set_opt(*_impl, CURLOPT_MIMEPOST, handle);
}

Result<void> Request::clear_mime()
{
    if (_impl->mime != nullptr)
    {
        curl_mime_free(_impl->mime);
        _impl->mime = nullptr;
    }
    return set_opt(*_impl, CURLOPT_MIMEPOST, (const void *)nullptr);
}

Result<void> Request::set_body(ArrCharView data)
{
    // an empty body still declares its size: without it curl has no
    // content-length and falls back to a chunked upload
    auto size_set = set_opt(*_impl, CURLOPT_POSTFIELDSIZE, (long)data.size());
    SIHD_UNEXPECTED_RETURN(size_set);
    // COPYPOSTFIELDS must come after POSTFIELDSIZE to copy binary data fully
    return set_opt(*_impl, CURLOPT_COPYPOSTFIELDS, data.buf());
}

Result<void> Request::set_upload(bool on)
{
    return set_opt(*_impl, CURLOPT_UPLOAD, (long)on);
}

Result<void> Request::set_infilesize(int64_t size)
{
    return set_opt(*_impl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)size);
}

Result<void> Request::set_http_get(bool on)
{
    return set_opt(*_impl, CURLOPT_HTTPGET, (long)on);
}

Result<void> Request::set_http_post(bool on)
{
    return set_opt(*_impl, CURLOPT_POST, (long)on);
}

Result<void> Request::set_nobody(bool on)
{
    return set_opt(*_impl, CURLOPT_NOBODY, (long)on);
}

Result<void> Request::set_custom_request(std::string_view method)
{
    _impl->custom_request = std::string(method);
    return set_opt(*_impl, CURLOPT_CUSTOMREQUEST, _impl->custom_request.c_str());
}

Result<void> Request::set_accept_encoding(std::string_view encoding)
{
    _impl->accept_encoding = std::string(encoding);
    return set_opt(*_impl, CURLOPT_ACCEPT_ENCODING, _impl->accept_encoding.c_str());
}

Result<void> Request::clear_accept_encoding()
{
    // an empty encoding enables every supported one: only a null value disables
    return set_opt(*_impl, CURLOPT_ACCEPT_ENCODING, (const char *)nullptr);
}

Result<void> Request::set_http_2_tls(bool on)
{
    if (on)
        return set_opt(*_impl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    return set_opt(*_impl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
}

Result<void> Request::set_cookie_engine()
{
    _impl->cookies_file = "";
    return set_opt(*_impl, CURLOPT_COOKIEFILE, _impl->cookies_file.c_str());
}

Result<void> Request::add_cookie(std::string_view cookie_line)
{
    _impl->cookie_line = std::string(cookie_line);
    return set_opt(*_impl, CURLOPT_COOKIELIST, _impl->cookie_line.c_str());
}

Result<void> Request::save_cookies(std::string_view path)
{
    _impl->cookie_jar = std::string(path);
    auto jar_set = set_opt(*_impl, CURLOPT_COOKIEJAR, _impl->cookie_jar.c_str());
    SIHD_UNEXPECTED_RETURN(jar_set);
    return add_cookie("FLUSH");
}

Result<void> Request::load_cookies(std::string_view path)
{
    _impl->cookies_file = std::string(path);
    return set_opt(*_impl, CURLOPT_COOKIEFILE, _impl->cookies_file.c_str());
}

Result<void> Request::clear_cookies()
{
    return add_cookie("ALL");
}

Result<void> Request::set_write_callback(WriteFn callback)
{
    _impl->callbacks.write = std::move(callback);
    auto fn_set = set_opt(*_impl, CURLOPT_WRITEFUNCTION, data_trampoline<&Callbacks::write>);
    SIHD_UNEXPECTED_RETURN(fn_set);
    return set_opt(*_impl, CURLOPT_WRITEDATA, &_impl->callbacks);
}

Result<void> Request::set_header_callback(WriteFn callback)
{
    _impl->callbacks.header = std::move(callback);
    auto fn_set = set_opt(*_impl, CURLOPT_HEADERFUNCTION, data_trampoline<&Callbacks::header>);
    SIHD_UNEXPECTED_RETURN(fn_set);
    return set_opt(*_impl, CURLOPT_HEADERDATA, &_impl->callbacks);
}

Result<void> Request::set_read_callback(ReadFn callback)
{
    _impl->callbacks.read = std::move(callback);
    auto fn_set = set_opt(*_impl, CURLOPT_READFUNCTION, read_trampoline);
    SIHD_UNEXPECTED_RETURN(fn_set);
    return set_opt(*_impl, CURLOPT_READDATA, &_impl->callbacks);
}

Result<void> Request::set_progress_callback(ProgressFn callback)
{
    _impl->callbacks.progress = std::move(callback);
    if (!_impl->callbacks.progress)
        return set_opt(*_impl, CURLOPT_NOPROGRESS, 1L);
    auto progress_set = set_opt(*_impl, CURLOPT_NOPROGRESS, 0L);
    SIHD_UNEXPECTED_RETURN(progress_set);
    auto fn_set = set_opt(*_impl, CURLOPT_XFERINFOFUNCTION, progress_trampoline);
    SIHD_UNEXPECTED_RETURN(fn_set);
    return set_opt(*_impl, CURLOPT_XFERINFODATA, &_impl->callbacks);
}

long Request::response_code() const
{
    long code = 0;
    get_info(_impl->curl, CURLINFO_RESPONSE_CODE, code);
    return code;
}

std::string Request::content_type() const
{
    char *content_type = nullptr;
    if (get_info(_impl->curl, CURLINFO_CONTENT_TYPE, content_type) && content_type != nullptr)
        return content_type;
    return "";
}

std::string Request::redirect_url() const
{
    char *redirect_url = nullptr;
    if (get_info(_impl->curl, CURLINFO_REDIRECT_URL, redirect_url) && redirect_url != nullptr)
        return redirect_url;
    return "";
}

long Request::new_connection_count() const
{
    long count = 0;
    get_info(_impl->curl, CURLINFO_NUM_CONNECTS, count);
    return count;
}

std::vector<std::string> Request::cookie_list() const
{
    curl_slist *raw = nullptr;
    if (!get_info(_impl->curl, CURLINFO_COOKIELIST, raw))
        return {};
    std::unique_ptr<curl_slist, SlistFree> list(raw);
    return slist_lines(list.get());
}

Result<void> Request::perform()
{
    if (_impl->curl == nullptr)
        return err(not_initialized, _impl->init_error.empty() ? "no curl handle" : _impl->init_error);
    _impl->code = curl_easy_perform(_impl->curl);
    if (_impl->code != CURLE_OK)
        return err(error_from(_impl->code), "perform '{}': {}", _impl->url, curl_easy_strerror(_impl->code));
    return {};
}

std::string Request::last_error() const
{
    return curl_easy_strerror(_impl->code);
}

void Request::reset()
{
    if (_impl->curl == nullptr)
        return;
    curl_easy_reset(_impl->curl);
    _impl->header_list.reset();
    _impl->resolve_list.reset();
    if (_impl->mime != nullptr)
    {
        curl_mime_free(_impl->mime);
        _impl->mime = nullptr;
    }
    _impl->code = CURLE_OK;
}

} // namespace sihd::curl
