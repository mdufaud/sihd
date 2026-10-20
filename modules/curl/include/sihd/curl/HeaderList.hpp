#ifndef __SIHD_CURL_HEADERLIST_HPP__
#define __SIHD_CURL_HEADERLIST_HPP__

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Result.hpp>

namespace sihd::curl
{

struct HeaderListImpl;

class HeaderList
{
    public:
        HeaderList();
        ~HeaderList();

        HeaderList(HeaderList &&);
        HeaderList & operator=(HeaderList &&);

        sihd::util::Result<void> append(std::string_view line);
        bool empty() const;
        size_t size() const;
        std::vector<std::string> lines() const;

    private:
        HeaderListImpl *_impl = nullptr;
};

} // namespace sihd::curl

#endif
