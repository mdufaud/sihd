#ifndef __SIHD_HTTP_HTTPHEADER_HPP__
#define __SIHD_HTTP_HTTPHEADER_HPP__

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <sihd/util/Array.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::http
{

class HttpHeader
{
    public:
        using HeaderMap = std::unordered_map<std::string, std::string>;

        HttpHeader();
        HttpHeader(HeaderMap && headers);
        HttpHeader(HttpHeader &&) = default;
        HttpHeader & operator=(HttpHeader &&) = default;
        virtual ~HttpHeader();

        HttpHeader & set_server(std::string_view name);
        HttpHeader & set_content_type(std::string_view type);
        HttpHeader & set_content_type(std::string_view type, std::string_view charset);
        HttpHeader & set_content_length(size_t len);
        HttpHeader & set_accept_charset(std::string_view charset);
        HttpHeader & set_accept(std::string_view mime_type);

        HttpHeader & set_headers(HeaderMap && headers);
        HttpHeader & set_header(const std::string & name, std::string_view value);
        std::expected<void, sihd::util::Error> add_header_from_str(std::string_view header_str);

        HttpHeader & remove_header(const std::string & name);

        std::optional<size_t> content_length() const;
        std::optional<std::string_view> accept_charset() const;
        std::optional<std::string_view> content_type() const;
        std::optional<std::string_view> server() const;

        const HeaderMap & headers() const { return _headers; }
        std::string_view find(const std::string & header_name) const;
        const std::string & get(const std::string & header_name) const;

    protected:

    private:
        HeaderMap _headers;
};

std::optional<std::string> header_param(std::string_view value, std::string_view param);

} // namespace sihd::http

#endif