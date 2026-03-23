#ifndef __SIHD_HTTP_HTTPREQUEST_HPP__
#define __SIHD_HTTP_HTTPREQUEST_HPP__

#include <optional>
#include <string>
#include <unordered_map>

#include <sihd/json/fwd.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>

namespace sihd::http
{

class HttpRequest
{
    public:
        enum RequestType
        {
            None = -1,
            Post = 0,
            Get = 1,
            Put = 2,
            Delete = 3,
            Options = 4,
        };

        HttpRequest(std::string_view url, RequestType request_type = Get);
        HttpRequest(std::string_view url,
                    const std::vector<std::string> & uri_args,
                    RequestType request_type = Get);
        virtual ~HttpRequest();

        static RequestType type_from_str(std::string_view type);
        static std::string type_str(RequestType type);
        std::string type_str() const;

        bool has_content() const;
        void set_content(sihd::util::ArrCharView data);
        void set_client_ip(const std::string & ip);
        void set_path_params(std::unordered_map<std::string, std::string> && params);
        void set_query_params(std::unordered_map<std::string, std::string> && params);
        void set_auth_user(std::string_view user);
        void set_auth_token(std::string_view token);
        void set_cookie(std::string_view name, std::string_view value);

        sihd::json::Json content_as_json() const;

        std::optional<std::string_view> path_param(const std::string & name) const;
        std::optional<std::string_view> query_param(const std::string & name) const;
        std::optional<std::string_view> cookie(const std::string & name) const;

        const std::string & client_ip() const { return _ip; }
        const sihd::util::ArrChar & content() const { return _array; }
        const std::string & url() const { return _url; }
        const std::vector<std::string> & uri_args() const { return _uri_args_lst; }
        const std::unordered_map<std::string, std::string> & path_params() const { return _path_params; }
        const std::unordered_map<std::string, std::string> & query_params() const { return _query_params; }
        const std::unordered_map<std::string, std::string> & cookies() const { return _cookies; }
        RequestType request_type() const { return _request_type; }
        const std::string & auth_user() const { return _auth_user; }
        const std::string & auth_token() const { return _auth_token; }
        bool is_authenticated() const { return !_auth_user.empty() || !_auth_token.empty(); }

    protected:

    private:
        RequestType _request_type;
        std::string _url;
        std::vector<std::string> _uri_args_lst;
        std::unordered_map<std::string, std::string> _path_params;
        std::unordered_map<std::string, std::string> _query_params;
        std::string _ip;
        std::string _auth_user;
        std::string _auth_token;
        std::unordered_map<std::string, std::string> _cookies;
        sihd::util::ArrChar _array;
};

} // namespace sihd::http

#endif