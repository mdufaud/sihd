#include <sihd/util/DynLib.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>

#define SIHD_UTIL_NAMEDFACTORY_PREFIX "sihd_util_namedfactory_"

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Named);

SIHD_LOGGER;

Named *NamedFactory::load(const std::string & libname,
                          const std::string & classname,
                          const std::string & name,
                          Node *parent)
{
    Named *(*constructor_sym)(std::string, Node *);
    std::string factory_name = SIHD_UTIL_NAMEDFACTORY_PREFIX + classname;
    DynLib lib(libname);
    if (lib.is_open() == false)
        return nullptr;
    constructor_sym = (Named * (*)(std::string, Node *)) lib.load(factory_name);
    if (constructor_sym == nullptr)
        return nullptr;
    return constructor_sym(name, parent);
}

} // namespace sihd::util