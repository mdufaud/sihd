#ifndef __SIHD_HTTP_HTTPHEADER_HPP__
# define __SIHD_HTTP_HTTPHEADER_HPP__

# include <sihd/util/Array.hpp>

namespace sihd::http
{

class HttpHeader
{
    public:
        HttpHeader();
        virtual ~HttpHeader();

        HttpHeader & set_server_name(std::string_view name);

        HttpHeader & set_status(uint32_t status);
        HttpHeader & set_content_type(std::string_view type);
        HttpHeader & set_content_size(size_t len);
        HttpHeader & set_charset(std::string_view encoding);

        HttpHeader & set_status_content(uint32_t status, std::string_view content_type, size_t content_len);

        HttpHeader & set_header(const std::string & name, std::string_view value);
        HttpHeader & remove_header(const std::string & name);
        HttpHeader & set_header(const unsigned char *name, std::string_view value);
        HttpHeader & remove_header(const unsigned char *name);

        uint32_t status() const { return _status; }
        size_t content_size() const { return _content_size; }
        const std::string & encoding() const { return _encoding; }
        const std::string & content_type() const { return _content_type; }
        const std::string & server_name() const { return _server_name; }
        const std::unordered_map<std::string, std::string> & headers() const { return _headers; }

        static std::string build_content_type(std::string_view type, std::string_view encoding = "");

    protected:

    private:
        std::string _server_name;
        uint32_t _status;
        std::string _content_type;
        std::string _encoding;
        size_t _content_size;
        std::unordered_map<std::string, std::string> _headers;
};

}

#endif