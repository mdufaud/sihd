#ifndef __SIHD_HTTP_NAVIGATOR_MULTIPARTFIELD_HPP__
#define __SIHD_HTTP_NAVIGATOR_MULTIPARTFIELD_HPP__

#include <string>

namespace sihd::http
{

struct MultipartField
{
        std::string name;
        std::string value;
        bool is_file = false;
        std::string content_type;
        std::string filename;
};

} // namespace sihd::http

#endif
