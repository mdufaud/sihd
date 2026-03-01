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
    _callback_manager_map[type].set<void, const HttpRequest &, HttpResponse &>(path, fun);
}

bool WebService::call(const std::string & path, const HttpRequest & request, HttpResponse & response)
{
    sihd::util::CallbackManager & callback_manager = _callback_manager_map[request.request_type()];
    if (callback_manager.exists(path) == false)
        return false;
    callback_manager.call<void, const HttpRequest &, HttpResponse &>(path, request, response);
    return true;
}

} // namespace sihd::http