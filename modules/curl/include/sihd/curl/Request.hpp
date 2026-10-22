#ifndef __SIHD_CURL_REQUEST_HPP__
#define __SIHD_CURL_REQUEST_HPP__

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::curl
{

struct XferProgress
{
        long dltotal = 0;
        long dlnow = 0;
        long ultotal = 0;
        long ulnow = 0;
};

enum class Auth
{
    Basic,
    Digest,
    Any,
};

enum class Proxy
{
    Http,
    Socks4,
    Socks5,
};

class HeaderList;
class Mime;

// Keep a request around to reuse its connection pool; never at static storage duration (curl exits early)
class Request
{
    public:
        // consumes received data; returns false to abort the transfer
        using WriteFn = std::function<bool(sihd::util::ArrByteView)>;
        // fills the buffer, returns the number of bytes written; 0 ends the data
        using ReadFn = std::function<size_t(char *buffer, size_t capacity)>;
        // returns false to abort the transfer
        using ProgressFn = std::function<bool(XferProgress)>;

        // a read callback returns it to abort the transfer instead of ending the data
        static constexpr size_t READ_ABORT = 0x10000000;

        Request();
        ~Request();

        Request(const Request &) = delete;
        Request & operator=(const Request &) = delete;
        // the moved-from request keeps a defunct handle: every operation fails
        Request(Request &&);
        Request & operator=(Request &&);

        sihd::util::Result<void> set_url(std::string_view url);
        sihd::util::Result<void> set_verbose(bool on);
        sihd::util::Result<void> set_follow_location(bool on);
        // no addresses clears the entry
        sihd::util::Result<void>
            set_resolve(std::string_view host, int port, const std::vector<std::string> & addresses);
        sihd::util::Result<void> set_timeout(sihd::util::Duration duration);
        sihd::util::Result<void> set_connect_timeout(sihd::util::Duration duration);
        sihd::util::Result<void> set_user_agent(std::string_view user_agent);
        sihd::util::Result<void> set_ssl_verify(bool verify_peer, bool verify_host);
        sihd::util::Result<void> set_userpass(std::string_view user, std::string_view password);
        sihd::util::Result<void> set_auth(Auth auth);
        // empty url disables any proxy, including the environment's
        sihd::util::Result<void> set_proxy(std::string_view url, Proxy type = Proxy::Http);
        // unlike set_proxy(""), the proxy comes back from the environment
        sihd::util::Result<void> clear_proxy();
        sihd::util::Result<void> set_proxy_auth(std::string_view user, std::string_view password);
        sihd::util::Result<void> set_headers(const HeaderList & headers);
        // parts are copied into the transfer handle
        sihd::util::Result<void> set_mime(const Mime & mime);
        sihd::util::Result<void> clear_mime();
        // the body is copied by the transfer handle
        sihd::util::Result<void> set_body(sihd::util::ArrCharView data);
        sihd::util::Result<void> set_upload(bool on);
        sihd::util::Result<void> set_infilesize(int64_t size);
        sihd::util::Result<void> set_http_get(bool on);
        sihd::util::Result<void> set_http_post(bool on);
        sihd::util::Result<void> set_nobody(bool on);
        sihd::util::Result<void> set_custom_request(std::string_view method);
        // empty encoding enables every supported one
        sihd::util::Result<void> set_accept_encoding(std::string_view encoding);
        sihd::util::Result<void> clear_accept_encoding();
        sihd::util::Result<void> set_http_2_tls(bool on);

        sihd::util::Result<void> set_cookie_engine();
        sihd::util::Result<void> add_cookie(std::string_view cookie_line);
        sihd::util::Result<void> save_cookies(std::string_view path);
        sihd::util::Result<void> load_cookies(std::string_view path);
        sihd::util::Result<void> clear_cookies();

        sihd::util::Result<void> set_write_callback(WriteFn callback);
        sihd::util::Result<void> set_header_callback(WriteFn callback);
        sihd::util::Result<void> set_read_callback(ReadFn callback);
        sihd::util::Result<void> set_progress_callback(ProgressFn callback);

        long response_code() const;
        std::string content_type() const;
        std::string redirect_url() const;
        // connections opened by the last perform(); 0 means one was reused
        long new_connection_count() const;
        std::vector<std::string> cookie_list() const;

        sihd::util::Result<void> perform();
        // error of the last failed operation, option setting or transfer, since the last reset()
        std::string last_error() const;
        void reset();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::curl

#endif
