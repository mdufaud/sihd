#ifndef __SIHD_HTTP_NAVIGATORRESPONSE_HPP__
#define __SIHD_HTTP_NAVIGATORRESPONSE_HPP__

#include <map>
#include <string>
#include <vector>

#include <sihd/http/HttpResponse.hpp>

namespace sihd::http
{

class NavigatorResponse: public HttpResponse
{
    public:
        NavigatorResponse();
        NavigatorResponse(HttpResponse && response);
        ~NavigatorResponse();

        NavigatorResponse(NavigatorResponse && other) = default;
        NavigatorResponse & operator=(NavigatorResponse && other) = default;

        const std::map<std::string, std::string> & cookies() const;
        const std::vector<std::string> & redirect_history() const;
        const std::string & final_url() const;

        bool was_method_downgraded() const;
        // why the redirect this response answers was not followed, empty when it was not refused
        const std::string & redirect_refusal() const;
        bool truncated() const;

        void set_cookies(std::map<std::string, std::string> && cookies);
        void set_redirect_history(std::vector<std::string> && history);
        void set_final_url(std::string && url);
        void set_method_downgraded(bool downgraded);
        void set_redirect_refusal(std::string && reason);
        void set_truncated(bool truncated);

    private:
        std::map<std::string, std::string> _cookies;
        std::vector<std::string> _redirect_history;
        std::string _final_url;
        std::string _redirect_refusal;
        bool _method_downgraded = false;
        bool _truncated = false;
};

} // namespace sihd::http

#endif
