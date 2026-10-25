#include <algorithm>
#include <array>
#include <iterator>

#include <sihd/curl/utils.hpp>
#include <sihd/http/navigator/LinkExtractor.hpp>

#include "HtmlScanner.hpp"

using namespace sihd::util;

namespace sihd::http
{

namespace
{

struct LinkTag
{
        std::string_view name;
        std::string_view url_attr;
};

constexpr auto link_tags = std::to_array<LinkTag>({{"a", "href"}, {"img", "src"}, {"link", "href"}, {"script", "src"}});

std::string attribute_of(const htmlscan::Tag & tag, std::string_view name)
{
    return std::string(tag.attribute(name).value_or(std::string_view()));
}

} // namespace

std::vector<ExtractedLink> extract_links(std::string_view html, ZStringView base_url)
{
    std::array<std::vector<ExtractedLink>, link_tags.size()> found;

    htmlscan::scan_tags(html, [&](const htmlscan::Tag & tag) {
        const auto link_tag = std::ranges::find(link_tags, tag.name, &LinkTag::name);
        if (link_tag == link_tags.end())
            return;
        std::string href = attribute_of(tag, link_tag->url_attr);
        if (href.empty())
            return;

        ExtractedLink link;
        link.url = sihd::curl::resolve_url(base_url, href).value_or(href);
        link.tag = tag.name;
        link.rel = attribute_of(tag, "rel");
        if (link.tag == "a")
            link.text = tag.trailing_text;
        else if (link.tag == "img")
            link.text = attribute_of(tag, "alt");
        found[static_cast<size_t>(link_tag - link_tags.begin())].push_back(std::move(link));
    });

    std::vector<ExtractedLink> result;
    for (std::vector<ExtractedLink> & links : found)
        std::move(links.begin(), links.end(), std::back_inserter(result));
    return result;
}

} // namespace sihd::http
