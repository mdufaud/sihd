#ifndef __SIHD_HTTP_NAVIGATORRESPONSE_HPP__
#define __SIHD_HTTP_NAVIGATORRESPONSE_HPP__

#include <map>
#include <string>
#include <vector>

#include <sihd/http/HttpResponse.hpp>

namespace sihd::http
{

class NavigatorResponse
{
    public:
        NavigatorResponse();
        NavigatorResponse(HttpResponse && response);
        ~NavigatorResponse();

        NavigatorResponse(NavigatorResponse && other) = default;
        NavigatorResponse & operator=(NavigatorResponse && other) = default;

        uint32_t status() const;
        const sihd::util::ArrByte & content() const;
        const HttpHeader & http_header() const;

        HttpResponse & response();
        const HttpResponse & response() const;

        const std::map<std::string, std::string> & cookies() const;
        const std::vector<std::string> & redirect_history() const;
        const std::string & final_url() const;

        void set_cookies(std::map<std::string, std::string> && cookies);
        void set_redirect_history(std::vector<std::string> && history);
        void set_final_url(std::string && url);

    private:
        HttpResponse _response;
        std::map<std::string, std::string> _cookies;
        std::vector<std::string> _redirect_history;
        std::string _final_url;
};

} // namespace sihd::http

#endif
