#ifndef __SIHD_HTTP_CURLOPTIONS_HPP__
#define __SIHD_HTTP_CURLOPTIONS_HPP__

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace sihd::http
{

struct CurlFileOptions
{
        std::string form_name;
        std::string file_path;
        std::string file_name;
        std::vector<uint8_t> data;
};

struct CurlOptions
{
        bool verbose = false;
        bool follow_location = false;
        long timeout_s = 10;
        bool ssl_verify_peer = false;
        bool ssl_verify_host = false;
        std::map<std::string, std::string> parameters = {};
        std::map<std::string, std::string> headers = {};
        std::string username = {};
        std::string password = {};
        std::string token = {};
        std::string user_agent = {};

        std::optional<CurlFileOptions> file = {};
        std::map<std::string, std::string> form_parameters = {};

        // long dltotal, long dlnow, long ultotal, long ulnow
        // dltotal is the total number of bytes libcurl expects to download in this transfer.
        // dlnow is the number of bytes downloaded so far.
        // ultotal is the total number of bytes libcurl expects to upload in this transfer.
        // ulnow is the number of bytes uploaded so far.
        std::function<bool(long, long, long, long)> upload_progress = {};

        static CurlOptions none() { return CurlOptions {}; }
};

} // namespace sihd::http

#endif
