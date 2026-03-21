#include <sihd/util/Logger.hpp>

#include <sihd/http/NavigatorResponse.hpp>

namespace sihd::http
{

SIHD_LOGGER;

NavigatorResponse::NavigatorResponse() = default;

NavigatorResponse::NavigatorResponse(HttpResponse && response): _response(std::move(response)) {}

NavigatorResponse::~NavigatorResponse() = default;

uint32_t NavigatorResponse::status() const
{
    // const_cast needed because HttpResponse::status() is not const
    return const_cast<HttpResponse &>(_response).status();
}

const sihd::util::ArrByte & NavigatorResponse::content() const
{
    return _response.content();
}

const HttpHeader & NavigatorResponse::http_header() const
{
    return _response.http_header();
}

HttpResponse & NavigatorResponse::response()
{
    return _response;
}

const HttpResponse & NavigatorResponse::response() const
{
    return _response;
}

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

} // namespace sihd::http
