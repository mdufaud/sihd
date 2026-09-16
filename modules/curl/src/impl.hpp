#ifndef __SIHD_CURL_SRC_IMPL_HPP__
#define __SIHD_CURL_SRC_IMPL_HPP__

#include <cstddef>
#include <string>
#include <vector>

#include <curl/curl.h>

namespace sihd::curl
{

struct HeaderListImpl
{
        curl_slist *list = nullptr;
        size_t size = 0;
};

struct MimeImpl
{
        curl_mime *mime = nullptr;
        // keeps every part wrapper addressable for the lifetime of the mime
        std::vector<struct MimePartImpl> parts;
};

// opaque wrapper around the curl part, no ownership: the mime frees its parts
struct MimePartImpl
{
        curl_mimepart *part = nullptr;
};

std::vector<std::string> slist_lines(const curl_slist *list);

} // namespace sihd::curl

#endif
