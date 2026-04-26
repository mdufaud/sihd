#ifndef __SIHD_HTTP_SRC_CURL_GLOBAL_HPP__
#define __SIHD_HTTP_SRC_CURL_GLOBAL_HPP__

// Internal header
// Provides a single curl global initialisation point
//
// curl_global_init() is not thread-safe and must not be called concurrently.
// Calling it multiple times from the same thread is defined behaviour (the
// second call is a no-op in current libcurl), but we guard it with call_once
// so every translation unit funnels through this single site.

#include <mutex>
#include <stdexcept>

#include <curl/curl.h>
#include <fmt/core.h>

namespace sihd::http
{

inline void curl_init_once()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        const CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
        if (code != CURLE_OK)
        {
            throw std::runtime_error(fmt::format("curl_global_init failed: {}", curl_easy_strerror(code)));
        }
        // Register cleanup with atexit so that curl_global_cleanup() is called
        // exactly once when the process exits, regardless of which translation
        // unit initialised curl.
        std::atexit(curl_global_cleanup);
    });
}

} // namespace sihd::http

#endif
