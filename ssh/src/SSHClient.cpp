#include <sihd/ssh/SSHClient.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <libssh2.h>

namespace sihd::ssh
{

SIHD_UTIL_REGISTER_FACTORY(SSHClient)

NEW_LOGGER("sihd::ssh");

SSHClient::SSHClient(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

SSHClient::~SSHClient()
{
}

}