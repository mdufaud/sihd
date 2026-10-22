#ifndef __SIHD_SYS_LINUX_MIME_FETCH_HPP__
#define __SIHD_SYS_LINUX_MIME_FETCH_HPP__

#include <string>
#include <utility>
#include <vector>

namespace sihd::sys::internal
{

using MimeLabel = std::pair<std::string, std::string>;

// offered-labeled when nothing is wanted, else the wanted mimes the offer advertises
std::vector<MimeLabel> mimes_to_fetch(const std::vector<std::string> & offered,
                                      const std::vector<std::string_view> & wanted_mimes);

} // namespace sihd::sys::internal

#endif
