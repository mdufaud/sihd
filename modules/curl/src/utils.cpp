#include <expected>
#include <memory>

#include <curl/curl.h>

#include <sihd/curl/utils.hpp>

#include "impl.hpp"

using namespace sihd::util;

namespace sihd::curl
{

namespace
{

struct UrlFree
{
        void operator()(CURLU *url) const { curl_url_cleanup(url); }
};

struct CurlFree
{
        void operator()(char *str) const { curl_free(str); }
};

} // namespace

std::string version()
{
    const char *version_str = curl_version();
    return version_str != nullptr ? version_str : "";
}

Result<std::string> resolve_url(std::string_view base, std::string_view reference)
{
    std::unique_ptr<CURLU, UrlFree> url(curl_url());
    if (url == nullptr)
        return std::unexpected(Error(ErrorCode::out_of_memory, "cannot allocate a curl url"));
    if (curl_url_set(url.get(), CURLUPART_URL, std::string(base).c_str(), CURLU_GUESS_SCHEME) != CURLUE_OK)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid base url '{}'", base));
    if (curl_url_set(url.get(), CURLUPART_URL, std::string(reference).c_str(), 0) != CURLUE_OK)
        return std::unexpected(Error(ErrorCode::invalid_argument, "invalid url reference '{}'", reference));
    char *resolved = nullptr;
    if (curl_url_get(url.get(), CURLUPART_URL, &resolved, 0) != CURLUE_OK)
        return std::unexpected(Error(ErrorCode::io_error, "cannot rebuild the url"));
    return std::string(std::unique_ptr<char, CurlFree>(resolved).get());
}

std::vector<std::string> slist_lines(const curl_slist *list)
{
    std::vector<std::string> lines;
    for (const curl_slist *each = list; each != nullptr; each = each->next)
        lines.emplace_back(each->data);
    return lines;
}

} // namespace sihd::curl
