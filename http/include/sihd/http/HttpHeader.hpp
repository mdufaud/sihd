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
        void set_servername(const std::string & name);

        void set_status(uint32_t status);
        void set_content_type(const std::string & type);
        void set_content_size(size_t len);
        void set_charset(const std::string & encoding);

        void set_common(uint32_t status, const std::string & content_type, size_t content_len);

        void set_by_token(enum lws_token_indexes token, const std::string & value);
        void set_by_name(const std::string & name, const std::string & value);

        void remove_by_token(enum lws_token_indexes token);
        void remove_by_name(const std::string & name);

        bool finalize(struct lws *wsi);

        u_char *buf() const { return (u_char *)_array.cbuf(); }
        size_t size() const;

        static std::string build_content_type(const std::string & type, const std::string & encoding);

    protected:
    
    private:
        size_t _buffer_size;
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