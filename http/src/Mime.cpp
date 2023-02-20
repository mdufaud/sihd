#include <sihd/util/Logger.hpp>

#include <sihd/http/Mime.hpp>

namespace sihd::http
{

Mime::Mime()
{
    _types["jpg"] = MIME_IMAGE_JPEG;
    _types["jpeg"] = MIME_IMAGE_JPEG;
    _types["png"] = MIME_IMAGE_PNG;
    _types["gif"] = MIME_IMAGE_GIF;
    _types["js"] = MIME_APPLICATION_JAVASCRIPT;
    _types["json"] = MIME_APPLICATION_JSON;
    _types["tar"] = MIME_APPLICATION_TAR;
    _types["css"] = MIME_TEXT_CSS;
    _types["csv"] = MIME_TEXT_CSV;
    _types["htm"] = MIME_TEXT_HTML;
    _types["html"] = MIME_TEXT_HTML;
    _types["svg"] = MIME_IMAGE_SVG;
    _types["ico"] = "image/vnd.microsoft.icon";
    _types["otf"] = "application/x-font-otf";
    _types["ttf"] = "application/x-font-ttf";
}

Mime::~Mime() {}

std::string Mime::get(const std::string & ext) const
{
    auto it = _types.find(ext);
    if (it != _types.end())
        return it->second;
    return "text/" + ext;
}

void Mime::add(const std::string & ext, std::string_view content_type)
{
    _types[ext] = content_type;
}

} // namespace sihd::http