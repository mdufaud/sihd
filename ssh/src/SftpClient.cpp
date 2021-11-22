#include <sihd/ssh/SftpClient.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::ssh
{

SIHD_UTIL_REGISTER_FACTORY(SftpClient)

LOGGER;

SftpClient::SftpClient(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

SftpClient::~SftpClient()
{
}

}