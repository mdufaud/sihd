#ifndef __SIHD_CURL_HEADERLIST_HPP__
#define __SIHD_CURL_HEADERLIST_HPP__

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Error.hpp>

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

        // no embedded NUL byte: curl stores C strings
        std::expected<void, sihd::util::Error> append(std::string_view line);
        bool empty() const;
        size_t size() const;
        std::vector<std::string> lines() const;

    private:
        HeaderListImpl *_impl = nullptr;
};

} // namespace sihd::curl

#endif
