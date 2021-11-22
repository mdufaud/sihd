#include <sihd/ssh/SshUtils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::ssh
{

SIHD_UTIL_REGISTER_FACTORY(SshUtils)

LOGGER;

SshUtils::SshUtils(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

SshUtils::~SshUtils()
{
}

}