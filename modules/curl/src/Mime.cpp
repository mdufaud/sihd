#include <sihd/curl/Mime.hpp>

namespace sihd::curl
{

Mime::Part::Part(MimePart *part): _part(part) {}

Mime::Part Mime::add_part()
{
    _parts.emplace_back();
    return Part(&_parts.back());
}

Mime::Part & Mime::Part::name(std::string_view name)
{
    if (_part != nullptr)
        _part->name = std::string(name);
    return *this;
}

Mime::Part & Mime::Part::data(std::string_view data)
{
    if (_part != nullptr)
        _part->data.assign(data.begin(), data.end());
    return *this;
}

Mime::Part & Mime::Part::data(const std::vector<uint8_t> & data)
{
    if (_part != nullptr)
        _part->data = data;
    return *this;
}

Mime::Part & Mime::Part::file(std::string_view path)
{
    if (_part != nullptr)
        _part->path = std::string(path);
    return *this;
}

Mime::Part & Mime::Part::filename(std::string_view filename)
{
    if (_part != nullptr)
        _part->filename = std::string(filename);
    return *this;
}

Mime::Part & Mime::Part::content_type(std::string_view content_type)
{
    if (_part != nullptr)
        _part->content_type = std::string(content_type);
    return *this;
}

} // namespace sihd::curl
