#include <string>
#include <utility>

#include <sihd/curl/Mime.hpp>
#include <sihd/util/Logger.hpp>

#include "impl.hpp"

namespace sihd::curl
{

SIHD_LOGGER;

namespace
{

// curl_mime setters only report through their return code
bool mime_check(CURLcode code, std::string_view what)
{
    if (code == CURLE_OK)
        return true;
    SIHD_LOG(error, "Mime: could not {}: {}", what, curl_easy_strerror(code));
    return false;
}

} // namespace

Mime::Mime(): _impl(new MimeImpl()) {}

Mime::~Mime()
{
    if (_impl != nullptr)
    {
        if (_impl->mime != nullptr)
            curl_mime_free(_impl->mime);
        delete _impl;
    }
}

Mime::Mime(Mime && other): _impl(other._impl)
{
    other._impl = new MimeImpl();
}

Mime & Mime::operator=(Mime && other)
{
    if (this != &other)
        std::swap(_impl, other._impl);
    return *this;
}

Mime::Part Mime::add_part()
{
    if (_impl->mime == nullptr)
        return Part();
    _impl->parts.push_back(MimePartImpl {curl_mime_addpart(_impl->mime)});
    return Part(&_impl->parts.back());
}

Mime::Part & Mime::Part::name(std::string_view name)
{
    if (_part != nullptr)
    {
        std::string str(name);
        mime_check(curl_mime_name(_part->part, str.c_str()), "set part name");
    }
    return *this;
}

Mime::Part & Mime::Part::data(std::string_view data)
{
    if (_part != nullptr)
    {
        std::string str(data);
        mime_check(curl_mime_data(_part->part, str.data(), str.size()), "set part data");
    }
    return *this;
}

Mime::Part & Mime::Part::data(const std::vector<uint8_t> & data)
{
    if (_part != nullptr)
        mime_check(curl_mime_data(_part->part, (const char *)data.data(), data.size()), "set part data");
    return *this;
}

Mime::Part & Mime::Part::file(std::string_view path)
{
    if (_part != nullptr)
    {
        std::string str(path);
        mime_check(curl_mime_filedata(_part->part, str.c_str()), "set part file");
    }
    return *this;
}

Mime::Part & Mime::Part::filename(std::string_view filename)
{
    if (_part != nullptr)
    {
        std::string str(filename);
        mime_check(curl_mime_filename(_part->part, str.c_str()), "set part filename");
    }
    return *this;
}

Mime::Part & Mime::Part::content_type(std::string_view content_type)
{
    if (_part != nullptr)
    {
        std::string str(content_type);
        mime_check(curl_mime_type(_part->part, str.c_str()), "set part content type");
    }
    return *this;
}

} // namespace sihd::curl
