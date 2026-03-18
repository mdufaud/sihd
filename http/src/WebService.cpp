#include <sihd/util/Logger.hpp>
#include <sihd/sys/NamedFactory.hpp>

#include <sihd/http/WebService.hpp>

namespace sihd::http
{

SIHD_REGISTER_FACTORY(WebService)

SIHD_LOGGER;

WebService::WebService(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent) {}

WebService::~WebService() = default;

void WebService::set_entry_point(const std::string & path,
                                 std::function<void(const HttpRequest &, HttpResponse &)> fun,
                                 HttpRequest::RequestType type)
{
    _route_table.add(path, std::move(fun), type);
}

bool WebService::call(std::string_view path, HttpRequest & request, HttpResponse & response)
{
    auto result = _route_table.find(request.request_type(), path);
    if (!result.has_value())
        return false;
    if (result->match.matched)
        request.set_path_params(std::move(result->match.params));
    result->handler(request, response);
    return true;
}

} // namespace sihd::http