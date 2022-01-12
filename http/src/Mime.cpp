#include <sihd/http/Mime.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::http
{

const char *Mime::MIME_TEXT_PLAIN = "text/plain";
const char *Mime::MIME_TEXT_HTML = "text/html";
const char *Mime::MIME_TEXT_JAVASCRIPT = "text/javascript";
const char *Mime::MIME_TEXT_CSS = "text/css";
const char *Mime::MIME_TEXT_CSV = "text/csv";
const char *Mime::MIME_APPLICATION_OCTET = "application/octet-stream";
const char *Mime::MIME_APPLICATION_JAVASCRIPT = "application/javascript";
const char *Mime::MIME_APPLICATION_JSON = "application/json";
const char *Mime::MIME_APPLICATION_TAR = "application/x-tar";
const char *Mime::MIME_IMAGE_JPEG = "image/jpeg";
const char *Mime::MIME_IMAGE_PNG = "image/png";
const char *Mime::MIME_IMAGE_GIF = "image/gif";

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
    _types["svg"] = "image/svg+xml";
    _types["ico"] = "image/vnd.microsoft.icon";
    _types["otf"] = "application/x-font-otf";
    _types["ttf"] = "application/x-font-ttf";
}

Mime::~Mime()
{
}

std::string Mime::get(const std::string & ext) const
{
    auto it = _types.find(ext);
    if (it != _types.end())
        return it->second;
    return "text/" + ext;
}

void    Mime::add(const std::string & ext, const std::string & content_type)
{
    _types[ext] = content_type;
}

}