#ifndef __SIHD_HTTP_HTTPREQUEST_HPP__
# define __SIHD_HTTP_HTTPREQUEST_HPP__

# include <nlohmann/json.hpp>
# include <sihd/util/IArray.hpp>
# include <string>
# include <string_view>
# include <vector>
# include <map>

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
            REQ_DELETE = 3,
        };

        HttpRequest(const std::string & path, RequestType request_type = GET);
        HttpRequest(const std::string & path, const std::vector<std::string> & uri_args, RequestType request_type = GET);
        virtual ~HttpRequest();

        static RequestType request_from_string(std::string type);
        static std::string request_to_string(RequestType type);

        void set_content(sihd::util::IArray *array);

        // check for json.is_discarded() for parsing error
        nlohmann::json content_as_json() const;
        const sihd::util::IArray *content() const { return _array_content; }

        const std::string & path() const { return _path; }
        RequestType request_type() const { return _request_type; }

    protected:

    private:
        RequestType _request_type;
        std::string _path;
        std::vector<std::string> _uri_args_lst;
        sihd::util::IArray *_array_content;
};

}

#endif