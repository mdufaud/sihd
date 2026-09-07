#include <sihd/util/mime.hpp>

namespace sihd::util::mime
{

namespace
{

std::string_view base_type(std::string_view mime_str)
{
    const size_t semicolon = mime_str.find(';');
    return semicolon == std::string_view::npos ? mime_str : mime_str.substr(0, semicolon);
}

} // namespace

bool compatible(std::string_view a, std::string_view b)
{
    if (a.empty() || b.empty())
        return true;
    return base_type(a) == base_type(b);
}

std::optional<std::string_view> match_offered(const std::vector<std::string> & offered, std::string_view wanted)
{
    for (const std::string & offered_mime : offered)
    {
        if (compatible(offered_mime, wanted))
            return offered_mime;
    }
    return std::nullopt;
}

std::optional<std::string_view> best_match(const std::vector<std::string> & offered,
                                           const std::vector<std::string_view> & wanted)
{
    for (std::string_view wanted_mime : wanted)
    {
        if (match_offered(offered, wanted_mime).has_value())
            return wanted_mime;
    }
    return std::nullopt;
}

} // namespace sihd::util::mime
