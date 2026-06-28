#ifndef __SIHD_HTTP_NAVIGATOR_LINKEXTRACTOR_HPP__
#define __SIHD_HTTP_NAVIGATOR_LINKEXTRACTOR_HPP__

#include <string>
#include <string_view>
#include <vector>

namespace sihd::http
{

struct ExtractedLink
{
        std::string url;
        std::string text;
        std::string tag;
        std::string rel;
};

std::vector<ExtractedLink> extract_links(std::string_view html, std::string_view base_url = "");

} // namespace sihd::http

#endif
