#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Named);

LOGGER;

Named *NamedFactory::load(const std::string & libname,
                            const std::string & classname,
                            const std::string & name,
                            Node *parent)
{
    Named *(*constructor_sym)(std::string, Node *);
    std::string factory_name = SIHD_UTIL_NAMEDFACTORY_PREFIX + classname;
    constructor_sym = (Named *(*)(std::string, Node *))OS::load_symbol_unload_lib(libname, factory_name);
    if (constructor_sym == nullptr)
        return nullptr;
    return constructor_sym(name, parent);
}

}