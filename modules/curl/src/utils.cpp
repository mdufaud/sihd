#include <curl/curl.h>

#include <sihd/curl/utils.hpp>

#include "impl.hpp"

namespace sihd::curl
{

std::string version()
{
    const char *version_str = curl_version();
    return version_str != nullptr ? version_str : "";
}

std::vector<std::string> slist_lines(const curl_slist *list)
{
    std::vector<std::string> lines;
    for (const curl_slist *each = list; each != nullptr; each = each->next)
        lines.emplace_back(each->data);
    return lines;
}

} // namespace sihd::curl
