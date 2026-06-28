#ifndef __SIHD_HTTP_MIME_HPP__
#define __SIHD_HTTP_MIME_HPP__

#include <string>
#include <unordered_map>
#include <vector>

namespace sihd::http
{

class Mime
{
    public:
        Mime();
        virtual ~Mime();

        /**
         * Get the mimetype associated to given extension.
         * @param ext the file extension.
         * @return the mime type.
         */
        std::string get(const std::string & ext) const;

        /**
         * Add a new mime type.
         */
        void add(const std::string & ext, std::string_view content_type);

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

        static std::string make_mime(std::string_view mime_type,
                                     std::string_view sub_type,
                                     const std::vector<std::string> & sub_type_suffixes = {},
                                     float q_factor_weighting = 0.0);

    protected:

    private:
        std::unordered_map<std::string, std::string> _types;
};

} // namespace sihd::http

#endif