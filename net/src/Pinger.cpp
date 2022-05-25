#include <sihd/net/Pinger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::net
{

SIHD_UTIL_REGISTER_FACTORY(Pinger)

SIHD_LOGGER;

Pinger::Pinger(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent)
{
}

Pinger::~Pinger()
{
}

}