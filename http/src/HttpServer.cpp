#include <sihd/http/HttpServer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::http
{

SIHD_UTIL_REGISTER_FACTORY(HttpServer)

LOGGER;

HttpServer::HttpServer(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

HttpServer::~HttpServer()
{
}

}