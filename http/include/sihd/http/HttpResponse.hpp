#ifndef __SIHD_HTTP_HTTPRESPONSE_HPP__
# define __SIHD_HTTP_HTTPRESPONSE_HPP__

# include <nlohmann/json.hpp>

# include <sihd/util/ArrayView.hpp>

# include <sihd/http/Mime.hpp>
# include <sihd/http/HttpHeader.hpp>

namespace sihd::http
{

class HttpResponse
{
    public:
        HttpResponse(Mime *mimes = nullptr);
        virtual ~HttpResponse();

        bool set_content(sihd::util::ArrViewChar data);

        bool set_plain_content(std::string_view str);
        bool set_byte_content(sihd::util::ArrViewByte data);
        bool set_json_content(const nlohmann::json & data);

        void set_content_type(std::string_view mime_type);
        void set_content_type_from_extension(const std::string & extension);

        HttpHeader & http_header() { return _http_header; }
        const HttpHeader & http_header() const { return _http_header; }

        const sihd::util::ArrByte & content() const { return _array; }

    protected:

    private:
        void _set_mime_type_if_not_set(const char *type);

        HttpHeader _http_header;
        sihd::util::ArrByte _array;
        Mime *_mime_ptr;
};

}

#endif