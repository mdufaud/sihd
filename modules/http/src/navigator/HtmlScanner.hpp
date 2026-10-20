#ifndef __SIHD_HTTP_SRC_NAVIGATOR_HTMLSCANNER_HPP__
#define __SIHD_HTTP_SRC_NAVIGATOR_HTMLSCANNER_HPP__

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sihd::http::htmlscan
{

struct Attribute
{
        std::string name;
        std::string_view value;
};

struct Tag
{
        std::string name;
        std::vector<Attribute> attributes;
        std::string_view trailing_text;

        std::optional<std::string_view> attribute(std::string_view attribute_name) const;
};

// single forward pass (no backtracking): an unclosed quote or bracket ends the scan
void scan_tags(std::string_view markup, const std::function<void(const Tag &)> & on_tag);

} // namespace sihd::http::htmlscan

#endif
