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

std::vector<std::string> slist_lines(const curl_slist *list);

} // namespace sihd::curl

#endif
