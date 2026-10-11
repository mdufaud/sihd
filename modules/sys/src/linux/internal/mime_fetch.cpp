#include "mime_fetch.hpp"

#include <algorithm>

#include <sihd/util/mime.hpp>

namespace sihd::sys::internal
{

std::vector<MimeLabel> mimes_to_fetch(const std::vector<std::string> & offered,
                                      const std::vector<std::string_view> & wanted_mimes)
{
    std::vector<MimeLabel> fetch_list;
    if (wanted_mimes.empty())
    {
        fetch_list.reserve(offered.size());
        for (const std::string & mime_str : offered)
            fetch_list.emplace_back(mime_str, mime_str);
        return fetch_list;
    }

    for (std::string_view mime_str : wanted_mimes)
    {
        if (auto offered_mime = sihd::util::mime::match_offered(offered, mime_str))
        {
            if (std::ranges::contains(fetch_list, *offered_mime, &MimeLabel::first) == false)
                fetch_list.emplace_back(*offered_mime, std::string(mime_str));
        }
    }
    return fetch_list;
}

} // namespace sihd::sys::internal
