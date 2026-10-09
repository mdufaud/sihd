#ifndef __SIHD_HTTP_REQUESTOPTIONS_HPP__
#define __SIHD_HTTP_REQUESTOPTIONS_HPP__

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

#include <sihd/http/Multipart.hpp>
#include <sihd/util/Duration.hpp>

namespace sihd::http
{

enum class ProxyType
{
    Http,
    Socks4,
    Socks5,
};

// download and upload totals and progress, in bytes
struct Progress
{
        long download_total = 0;
        long download_now = 0;
        long upload_total = 0;
        long upload_now = 0;
};

struct RequestOptions
{
        bool verbose = false;
        bool follow_location = false;
        bool accept_encoding = true;
        bool http2 = false;
        sihd::util::Duration timeout = sihd::util::Duration(sihd::util::time::sec(30));
        sihd::util::Duration connect_timeout = sihd::util::Duration(sihd::util::time::sec(10));
        bool ssl_verify_peer = true;
        bool ssl_verify_host = true;
        size_t max_response_size = 0;
        std::map<std::string, std::string> parameters = {};
        std::map<std::string, std::string> headers = {};
        std::string username = {};
        std::string password = {};
        bool digest = false;
        std::string token = {};
        std::string user_agent = {};

        // nullopt: use environment proxy (default) - empty string: disable any proxy (overrides
        // environment) - non-empty: use given proxy url
        std::optional<std::string> proxy = {};
        ProxyType proxy_type = ProxyType::Http;
        std::string proxy_username = {};
        std::string proxy_password = {};

        Multipart multipart = {};

        // progress of the transfer: returning false aborts it
        std::function<bool(const Progress &)> progress = {};

        static RequestOptions none();
};

} // namespace sihd::http

#endif
