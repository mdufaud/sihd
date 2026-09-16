#ifndef __SIHD_HTTP_MIMETYPES_HPP__
#define __SIHD_HTTP_MIMETYPES_HPP__

#include <string>
#include <string_view>
#include <unordered_map>

namespace sihd::http
{

class MimeTypes
{
    public:
        MimeTypes();
        ~MimeTypes();

        // extension without its dot, case-insensitive: an empty or unknown one
        // answers application/octet-stream
        std::string get(std::string_view ext) const;
        void add(std::string_view ext, std::string_view content_type);

        static constexpr const char *MIME_TEXT_PLAIN = "text/plain";
        static constexpr const char *MIME_TEXT_HTML = "text/html";
        static constexpr const char *MIME_TEXT_JAVASCRIPT = "text/javascript";
        static constexpr const char *MIME_TEXT_CSS = "text/css";
        static constexpr const char *MIME_TEXT_CSV = "text/csv";
        static constexpr const char *MIME_APPLICATION_OCTET = "application/octet-stream";
        static constexpr const char *MIME_APPLICATION_JAVASCRIPT = "application/javascript";
        static constexpr const char *MIME_APPLICATION_TAR = "application/x-tar";
        static constexpr const char *MIME_APPLICATION_JSON = "application/json";
        static constexpr const char *MIME_IMAGE_JPEG = "image/jpeg";
        static constexpr const char *MIME_IMAGE_PNG = "image/png";
        static constexpr const char *MIME_IMAGE_GIF = "image/gif";
        static constexpr const char *MIME_IMAGE_SVG = "image/svg+xml";
        static constexpr const char *MIME_ANY = "*/*";

    protected:

    private:
        std::unordered_map<std::string, std::string> _types;
};

} // namespace sihd::http

#endif
