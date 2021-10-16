#include <sihd/core/Core.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

namespace sihd::core
{

SIHD_UTIL_REGISTER_FACTORY(Core)

LOGGER;

Core::Core(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent)
{
}

Core::~Core()
{
}

}