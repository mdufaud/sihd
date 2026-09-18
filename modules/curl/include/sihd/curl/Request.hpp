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

namespace sihd::curl
{

// Download and upload totals and progress, in bytes
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

// One transfer to or from an url. Reusing a request keeps its connection
// pool alive: keep this object around instead of recreating it per transfer.
// A request must not outlive process exit: curl global cleanup runs in the
// exit chain, so no instance at static storage duration.
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

        bool set_url(std::string_view url);
        bool set_verbose(bool on);
        bool set_follow_location(bool on);
        bool set_timeout(sihd::util::Duration duration);
        bool set_connect_timeout(sihd::util::Duration duration);
        bool set_user_agent(std::string_view user_agent);
        bool set_ssl_verify(bool verify_peer, bool verify_host);
        bool set_userpass(std::string_view user, std::string_view password);
        bool set_auth(Auth auth);
        // empty url disables any proxy, including the environment's
        bool set_proxy(std::string_view url, Proxy type = Proxy::Http);
        // nullopt-like state: proxy comes back from the environment
        bool clear_proxy();
        bool set_proxy_auth(std::string_view user, std::string_view password);
        bool set_headers(const HeaderList & headers);
        // parts are copied into the transfer handle
        bool set_mime(const Mime & mime);
        bool clear_mime();
        // the body is copied by the transfer handle
        bool set_body(sihd::util::ArrCharView data);
        bool set_upload(bool on);
        // total upload size, expected by some protocols (http content-length, smtp size...)
        bool set_infilesize(int64_t size);
        bool set_http_get(bool on);
        bool set_http_post(bool on);
        bool set_nobody(bool on);
        bool set_custom_request(std::string_view method);
        // empty encoding enables every supported one
        bool set_accept_encoding(std::string_view encoding);
        bool clear_accept_encoding();
        bool set_http_2_tls(bool on);

        bool set_cookie_engine();
        bool add_cookie(std::string_view cookie_line);
        bool save_cookies(std::string_view path);
        bool load_cookies(std::string_view path);
        bool clear_cookies();

        bool set_write_callback(WriteFn callback);
        bool set_header_callback(WriteFn callback);
        bool set_read_callback(ReadFn callback);
        bool set_progress_callback(ProgressFn callback);

        long response_code() const;
        std::string content_type() const;
        std::string redirect_url() const;
        // connections opened by the last perform(); 0 means one was reused
        long new_connection_count() const;
        std::vector<std::string> cookie_list() const;

        bool perform();
        // error of the last failed operation - option setting or transfer - since the last reset()
        std::string last_error() const;
        void reset();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::curl

#endif
