#include <sihd/http/WebService.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::http
{

SIHD_UTIL_REGISTER_FACTORY(WebService)

LOGGER;

WebService::WebService(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

WebService::~WebService()
{
}

}