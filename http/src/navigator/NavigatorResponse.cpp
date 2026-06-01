#include <sihd/http/navigator/NavigatorResponse.hpp>

namespace sihd::http
{

NavigatorResponse::NavigatorResponse() = default;

NavigatorResponse::NavigatorResponse(HttpResponse && response): HttpResponse(std::move(response)) {}

NavigatorResponse::~NavigatorResponse() = default;

const std::map<std::string, std::string> & NavigatorResponse::cookies() const
{
    return _cookies;
}

const std::vector<std::string> & NavigatorResponse::redirect_history() const
{
    return _redirect_history;
}

const std::string & NavigatorResponse::final_url() const
{
    return _final_url;
}

void NavigatorResponse::set_cookies(std::map<std::string, std::string> && cookies)
{
    _cookies = std::move(cookies);
}

void NavigatorResponse::set_redirect_history(std::vector<std::string> && history)
{
    _redirect_history = std::move(history);
}

void NavigatorResponse::set_final_url(std::string && url)
{
    _final_url = std::move(url);
}

bool NavigatorResponse::was_method_downgraded() const
{
    return _method_downgraded;
}

void NavigatorResponse::set_method_downgraded(bool downgraded)
{
    _method_downgraded = downgraded;
}

} // namespace sihd::http
