#ifndef __SIHD_HTTP_HTTPREQUEST_HPP__
#define __SIHD_HTTP_HTTPREQUEST_HPP__

#include <nlohmann/json_fwd.hpp>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>

namespace sihd::http
{

class HttpRequest
{
    public:
        enum RequestType
        {
            NONE = -1,
            POST = 0,
            GET = 1,
            PUT = 2,
            DELETE = 3,
        };

        HttpRequest(std::string_view url, RequestType request_type = GET);
        HttpRequest(std::string_view url, const std::vector<std::string> & uri_args, RequestType request_type = GET);
        virtual ~HttpRequest();

        static RequestType type_from_str(std::string_view type);
        static std::string type_str(RequestType type);
        std::string type_str() const;

        bool has_content() const;
        void set_content(sihd::util::ArrCharView data);
        void set_client_ip(const std::string & ip);

        // check for json.is_discarded() for parsing error
        nlohmann::json content_as_json() const;

        const std::string & client_ip() const { return _ip; }
        const sihd::util::ArrChar & content() const { return _array; }
        const std::string & url() const { return _url; }
        const std::vector<std::string> & uri_args() const { return _uri_args_lst; }
        RequestType request_type() const { return _request_type; }

    protected:

    private:
        RequestType _request_type;
        std::string _url;
        std::vector<std::string> _uri_args_lst;
        std::string _ip;
        sihd::util::ArrChar _array;
};

} // namespace sihd::http

#endif