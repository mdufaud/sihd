#ifndef __SIHD_HTTP_HTTPHEADER_HPP__
# define __SIHD_HTTP_HTTPHEADER_HPP__

# include <sihd/util/Array.hpp>
# include <libwebsockets.h>

namespace sihd::http
{

class HttpHeader
{
    public:
        HttpHeader();
        virtual ~HttpHeader();

        bool set_buffer_size(size_t size);
        void set_servername(std::string_view name);

        void set_status(uint32_t status);
        void set_content_type(std::string_view type);
        void set_content_size(size_t len);
        void set_charset(std::string_view encoding);

        void set_common(uint32_t status, std::string_view content_type, size_t content_len);

        void set_header_by_token(enum lws_token_indexes token, std::string_view value);
        void set_header(const std::string & name, std::string_view value);

        void remove_header_by_token(enum lws_token_indexes token);
        void remove_header(const std::string & name);

        bool finalize(struct lws *wsi);

        uint32_t status() const { return _status; }
        size_t content_size() const { return _content_size; }
        const std::string & encoding() const { return _encoding; }
        const std::string & content_type() const { return _content_type; }
        const std::string & servername() const { return _server_name; }

        u_char *buf() const { return (u_char *)_array.cbuf(); }
        size_t size() const;

        static std::string build_content_type(std::string_view type, std::string_view encoding);

    protected:

    private:
        sihd::util::ArrUByte _array;
        u_char *_ptr;

        std::string _server_name;

        uint32_t _status;
        std::string _content_type;
        std::string _encoding;
        size_t _content_size;
        std::map<enum lws_token_indexes, std::string> _headers_token;
        std::map<std::string, std::string> _headers_name;
};

}

#endif