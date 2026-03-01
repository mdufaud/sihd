#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/sys/PluginLoader.hpp>
#include <sihd/util/Logger.hpp>

#define SIHD_FACTORY_PREFIX "sihd_factory_"

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

sihd::util::Named *PluginLoader::load(const std::string & libname,
                                      const std::string & classname,
                                      const std::string & name,
                                      sihd::util::Node *parent)
{
    sihd::util::Named *(*constructor_sym)(std::string, sihd::util::Node *);
    std::string factory_name = SIHD_FACTORY_PREFIX + classname;
    DynLib lib(libname);
    if (lib.is_open() == false)
        return nullptr;
    constructor_sym = (sihd::util::Named * (*)(std::string, sihd::util::Node *)) lib.load(factory_name);
    if (constructor_sym == nullptr)
        return nullptr;
    return constructor_sym(name, parent);
}

} // namespace sihd::sys
