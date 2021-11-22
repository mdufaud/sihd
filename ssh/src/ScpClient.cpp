#include <sihd/ssh/ScpClient.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::ssh
{

SIHD_UTIL_REGISTER_FACTORY(ScpClient)

LOGGER;

ScpClient::ScpClient(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

ScpClient::~ScpClient()
{
}

}