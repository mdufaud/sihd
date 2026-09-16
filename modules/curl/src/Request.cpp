#include <mutex>
#include <stdexcept>

#include <fmt/core.h>

#include <sihd/curl/HeaderList.hpp>
#include <sihd/curl/Mime.hpp>
#include <sihd/curl/Request.hpp>
#include <sihd/util/AtExit.hpp>
#include <sihd/util/Logger.hpp>

#include "impl.hpp"

namespace sihd::curl
{

SIHD_NEW_LOGGER("sihd::curl");

namespace
{

struct GlobalCleanup: public sihd::util::IRunnable
{
        bool run() override
        {
            curl_global_cleanup();
            return true;
        }
};

// curl_global_init() is not thread-safe and must be called before any other
// curl function from a single thread: funnel every request creation here.
void global_init_once()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
        if (code != CURLE_OK)
            throw std::runtime_error(fmt::format("curl_global_init failed: {}", curl_easy_strerror(code)));
        // cleanup joins sihd's exit chain so it is ordered with the other
        // exit handlers instead of racing them in the C atexit chain
        if (sihd::util::atexit::install())
            sihd::util::atexit::add_handler(new GlobalCleanup());
        else
            std::atexit(curl_global_cleanup);
    });
}

// the implementation type is deduced: setters record every curl failure into
// it so last_error() covers option setting, not only perform()
template <typename Impl, typename T>
bool set_opt(Impl & impl, CURLoption option, const T & value)
{
    if (impl.curl == nullptr)
    {
        SIHD_LOG(error, "Request: no curl handle");
        return false;
    }
    CURLcode code = curl_easy_setopt(impl.curl, option, value);
    if (code != CURLE_OK)
    {
        impl.code = code;
        SIHD_LOG(error, "Request: could not set curl option: {}", curl_easy_strerror(code));
    }
    return code == CURLE_OK;
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

// the write and header callbacks share the same trampoline shape
template <Request::WriteFn Callbacks::*member>
size_t data_trampoline(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto *callbacks = static_cast<Callbacks *>(userdata);
    const Request::WriteFn & fn = callbacks->*member;
    if (!fn)
        return size * nmemb;
    sihd::util::ArrByteView view((const int8_t *)ptr, size * nmemb);
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

} // namespace

struct Request::Impl
{
        CURL *curl = nullptr;
        CURLcode code = CURLE_OK;

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
            if (curl != nullptr)
                curl_easy_cleanup(curl);
        }
};

Request::Request(): _impl(std::make_unique<Impl>())
{
    global_init_once();
    _impl->curl = curl_easy_init();
    if (_impl->curl == nullptr)
        throw std::runtime_error("could not initialize curl request");
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

bool Request::set_url(std::string_view url)
{
    _impl->url = std::string(url);
    return set_opt(*_impl, CURLOPT_URL, _impl->url.c_str());
}

bool Request::set_verbose(bool on)
{
    return set_opt(*_impl, CURLOPT_VERBOSE, (long)on);
}

bool Request::set_follow_location(bool on)
{
    return set_opt(*_impl, CURLOPT_FOLLOWLOCATION, (long)on);
}

bool Request::set_timeout(sihd::util::Duration duration)
{
    return set_opt(*_impl, CURLOPT_TIMEOUT_MS, (long)duration.milliseconds());
}

bool Request::set_connect_timeout(sihd::util::Duration duration)
{
    return set_opt(*_impl, CURLOPT_CONNECTTIMEOUT_MS, (long)duration.milliseconds());
}

bool Request::set_user_agent(std::string_view user_agent)
{
    if (user_agent.empty())
        // an empty agent means no header: only a null value clears the option
        return set_opt(*_impl, CURLOPT_USERAGENT, (const char *)nullptr);
    _impl->user_agent = std::string(user_agent);
    return set_opt(*_impl, CURLOPT_USERAGENT, _impl->user_agent.c_str());
}

bool Request::set_ssl_verify(bool verify_peer, bool verify_host)
{
    bool peer_ok = set_opt(*_impl, CURLOPT_SSL_VERIFYPEER, (long)verify_peer);
    bool host_ok = set_opt(*_impl, CURLOPT_SSL_VERIFYHOST, verify_host ? 2L : 0L);
    return peer_ok && host_ok;
}

bool Request::set_userpass(std::string_view user, std::string_view password)
{
    _impl->user = std::string(user);
    _impl->password = std::string(password);
    bool user_ok = set_opt(*_impl, CURLOPT_USERNAME, _impl->user.c_str());
    bool pass_ok = set_opt(*_impl, CURLOPT_PASSWORD, _impl->password.c_str());
    return user_ok && pass_ok;
}

bool Request::set_auth(Auth auth)
{
    long mask = CURLAUTH_ANY;
    if (auth == Auth::Basic)
        mask = CURLAUTH_BASIC;
    else if (auth == Auth::Digest)
        mask = CURLAUTH_DIGEST;
    return set_opt(*_impl, CURLOPT_HTTPAUTH, mask);
}

bool Request::set_proxy(std::string_view url, Proxy type)
{
    _impl->proxy_url = std::string(url);

    long proxy_type = CURLPROXY_HTTP;
    if (type == Proxy::Socks4)
        proxy_type = CURLPROXY_SOCKS4;
    else if (type == Proxy::Socks5)
        proxy_type = CURLPROXY_SOCKS5;

    bool url_ok = set_opt(*_impl, CURLOPT_PROXY, _impl->proxy_url.c_str());
    bool type_ok = set_opt(*_impl, CURLOPT_PROXYTYPE, proxy_type);
    return url_ok && type_ok;
}

bool Request::set_proxy_auth(std::string_view user, std::string_view password)
{
    _impl->proxy_user = std::string(user);
    _impl->proxy_password = std::string(password);
    bool user_ok = set_opt(*_impl, CURLOPT_PROXYUSERNAME, _impl->proxy_user.c_str());
    bool pass_ok = set_opt(*_impl, CURLOPT_PROXYPASSWORD, _impl->proxy_password.c_str());
    return user_ok && pass_ok;
}

bool Request::clear_proxy()
{
    // an empty url disables the proxy: only a null value restores the environment's
    bool url_ok = set_opt(*_impl, CURLOPT_PROXY, (const char *)nullptr);
    bool type_ok = set_opt(*_impl, CURLOPT_PROXYTYPE, (long)CURLPROXY_HTTP);
    return url_ok && type_ok;
}

bool Request::set_headers(const HeaderList & headers)
{
    return set_opt(*_impl, CURLOPT_HTTPHEADER, headers._impl->list);
}

bool Request::set_mime(const Mime & mime)
{
    return set_opt(*_impl, CURLOPT_MIMEPOST, mime._impl->mime);
}

bool Request::clear_mime()
{
    return set_opt(*_impl, CURLOPT_MIMEPOST, (const void *)nullptr);
}

bool Request::set_body(sihd::util::ArrCharView data)
{
    // an empty body still declares its size: without it curl has no
    // content-length and falls back to a chunked upload
    bool size_ok = set_opt(*_impl, CURLOPT_POSTFIELDSIZE, (long)data.size());
    // COPYPOSTFIELDS must come after POSTFIELDSIZE to copy binary data fully
    bool copy_ok = set_opt(*_impl, CURLOPT_COPYPOSTFIELDS, data.buf());
    return size_ok && copy_ok;
}

bool Request::set_upload(bool on)
{
    return set_opt(*_impl, CURLOPT_UPLOAD, (long)on);
}

bool Request::set_infilesize(int64_t size)
{
    return set_opt(*_impl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)size);
}

bool Request::set_http_get(bool on)
{
    return set_opt(*_impl, CURLOPT_HTTPGET, (long)on);
}

bool Request::set_http_post(bool on)
{
    return set_opt(*_impl, CURLOPT_POST, (long)on);
}

bool Request::set_nobody(bool on)
{
    return set_opt(*_impl, CURLOPT_NOBODY, (long)on);
}

bool Request::set_custom_request(std::string_view method)
{
    _impl->custom_request = std::string(method);
    return set_opt(*_impl, CURLOPT_CUSTOMREQUEST, _impl->custom_request.c_str());
}

bool Request::set_accept_encoding(std::string_view encoding)
{
    _impl->accept_encoding = std::string(encoding);
    return set_opt(*_impl, CURLOPT_ACCEPT_ENCODING, _impl->accept_encoding.c_str());
}

bool Request::clear_accept_encoding()
{
    // an empty encoding enables every supported one: only a null value disables
    return set_opt(*_impl, CURLOPT_ACCEPT_ENCODING, (const char *)nullptr);
}

bool Request::set_http_2_tls(bool on)
{
    if (on)
        return set_opt(*_impl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    return set_opt(*_impl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
}

bool Request::set_cookie_engine()
{
    _impl->cookies_file = "";
    return set_opt(*_impl, CURLOPT_COOKIEFILE, _impl->cookies_file.c_str());
}

bool Request::add_cookie(std::string_view cookie_line)
{
    _impl->cookie_line = std::string(cookie_line);
    return set_opt(*_impl, CURLOPT_COOKIELIST, _impl->cookie_line.c_str());
}

bool Request::save_cookies(std::string_view path)
{
    _impl->cookie_jar = std::string(path);
    bool jar_ok = set_opt(*_impl, CURLOPT_COOKIEJAR, _impl->cookie_jar.c_str());
    return jar_ok && add_cookie("FLUSH");
}

bool Request::load_cookies(std::string_view path)
{
    _impl->cookies_file = std::string(path);
    return set_opt(*_impl, CURLOPT_COOKIEFILE, _impl->cookies_file.c_str());
}

bool Request::clear_cookies()
{
    return add_cookie("ALL");
}

bool Request::set_write_callback(WriteFn callback)
{
    _impl->callbacks.write = std::move(callback);
    bool fn_ok = set_opt(*_impl, CURLOPT_WRITEFUNCTION, data_trampoline<&Callbacks::write>);
    bool data_ok = set_opt(*_impl, CURLOPT_WRITEDATA, &_impl->callbacks);
    return fn_ok && data_ok;
}

bool Request::set_header_callback(WriteFn callback)
{
    _impl->callbacks.header = std::move(callback);
    bool fn_ok = set_opt(*_impl, CURLOPT_HEADERFUNCTION, data_trampoline<&Callbacks::header>);
    bool data_ok = set_opt(*_impl, CURLOPT_HEADERDATA, &_impl->callbacks);
    return fn_ok && data_ok;
}

bool Request::set_read_callback(ReadFn callback)
{
    _impl->callbacks.read = std::move(callback);
    bool fn_ok = set_opt(*_impl, CURLOPT_READFUNCTION, read_trampoline);
    bool data_ok = set_opt(*_impl, CURLOPT_READDATA, &_impl->callbacks);
    return fn_ok && data_ok;
}

bool Request::set_progress_callback(ProgressFn callback)
{
    _impl->callbacks.progress = std::move(callback);
    // empty callback restores the default: no progress reporting
    if (!_impl->callbacks.progress)
        return set_opt(*_impl, CURLOPT_NOPROGRESS, 1L);
    bool noprogress_ok = set_opt(*_impl, CURLOPT_NOPROGRESS, 0L);
    bool fn_ok = set_opt(*_impl, CURLOPT_XFERINFOFUNCTION, progress_trampoline);
    bool data_ok = set_opt(*_impl, CURLOPT_XFERINFODATA, &_impl->callbacks);
    return noprogress_ok && fn_ok && data_ok;
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
    curl_slist *list = nullptr;
    if (!get_info(_impl->curl, CURLINFO_COOKIELIST, list))
        return {};
    std::vector<std::string> lines = slist_lines(list);
    if (list != nullptr)
        curl_slist_free_all(list);
    return lines;
}

bool Request::perform()
{
    if (_impl->curl == nullptr)
    {
        SIHD_LOG(error, "Request: no curl handle");
        return false;
    }
    _impl->code = curl_easy_perform(_impl->curl);
    return _impl->code == CURLE_OK;
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
    _impl->code = CURLE_OK;
}

Mime Request::new_mime()
{
    if (_impl->curl == nullptr)
        return Mime();
    Mime mime;
    mime._impl->mime = curl_mime_init(_impl->curl);
    return mime;
}

} // namespace sihd::curl
