#ifndef __SIHD_CURL_HEADERLIST_HPP__
#define __SIHD_CURL_HEADERLIST_HPP__

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

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
        // false when the line could not be appended
        bool append(std::string_view line);
        bool empty() const;
        size_t size() const;
        std::vector<std::string> lines() const;

    private:
        friend class Request;
        HeaderListImpl *_impl = nullptr;
};

} // namespace sihd::curl

#endif
