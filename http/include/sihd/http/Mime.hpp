#ifndef __SIHD_HTTP_MIME_HPP__
# define __SIHD_HTTP_MIME_HPP__

# include <string>
# include <map>

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
        void add(const std::string & ext, const std::string & content_type);

        /**
         * text/html
         */
        static const char *MIME_TEXT_HTML;
        /**
         * text/javascript
         */
        static const char *MIME_TEXT_JAVASCRIPT;
        /**
         * text/json
         */
        static const char *MIME_TEXT_JSON;
        /**
         * text/css
         */
        static const char *MIME_TEXT_CSS;
        /**
         * application/javascript
         */
        static const char *MIME_APPLICATION_JAVASCRIPT;
        /**
         * application/javascript
         */
        static const char *MIME_APPLICATION_JSON;
        /**
         * image/jpeg
         */
        static const char *MIME_IMAGE_JPEG;
        /**
         * image/png
         */
        static const char *MIME_IMAGE_PNG;
        /**
         * image/gif
         */
        static const char *MIME_IMAGE_GIF;

    protected:
    
    private:
        std::map<std::string, std::string> _types;
};

}

#endif