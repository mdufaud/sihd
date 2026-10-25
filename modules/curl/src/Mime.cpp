#include <sihd/curl/Mime.hpp>

namespace sihd::curl
{

Mime::Part::Part(MimePart *part): _part(part) {}

const std::vector<MimePart> & Mime::parts() const
{
    return _parts;
}

Mime::Part Mime::add_part()
{
    _parts.emplace_back();
    return Part(&_parts.back());
}

Mime::Part & Mime::Part::name(const std::string & name)
{
    if (_part != nullptr)
        _part->name = name;
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

Mime::Part & Mime::Part::file(const std::string & path)
{
    if (_part != nullptr)
        _part->path = path;
    return *this;
}

Mime::Part & Mime::Part::filename(const std::string & filename)
{
    if (_part != nullptr)
        _part->filename = filename;
    return *this;
}

Mime::Part & Mime::Part::content_type(const std::string & content_type)
{
    if (_part != nullptr)
        _part->content_type = content_type;
    return *this;
}

} // namespace sihd::curl
