#include <sihd/http/MimeTypes.hpp>
#include <sihd/util/str.hpp>

namespace sihd::http
{

MimeTypes::MimeTypes()
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

MimeTypes::~MimeTypes() = default;

std::string MimeTypes::get(std::string_view ext) const
{
    std::string lower_ext(ext);
    sihd::util::str::to_lower(lower_ext);
    const auto it = _types.find(lower_ext);
    if (it != _types.end())
        return it->second;
    return MIME_APPLICATION_OCTET;
}

void MimeTypes::add(std::string_view ext, std::string_view content_type)
{
    std::string lower_ext(ext);
    sihd::util::str::to_lower(lower_ext);
    _types[std::move(lower_ext)] = std::string(content_type);
}

} // namespace sihd::http
