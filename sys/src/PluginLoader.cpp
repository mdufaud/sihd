#include <mutex>
#include <unordered_map>

#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/sys/PluginLoader.hpp>
#include <sihd/util/Logger.hpp>

#define SIHD_FACTORY_PREFIX "sihd_factory_"

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

std::mutex g_libs_mutex;
std::unordered_map<std::string, DynLib> g_loaded_libs;

} // namespace

sihd::util::Named *PluginLoader::load(const std::string & libname,
                                      const std::string & classname,
                                      const std::string & name,
                                      sihd::util::Node *parent)
{
    sihd::util::Named *(*constructor_sym)(std::string, sihd::util::Node *);
    std::string factory_name = SIHD_FACTORY_PREFIX + classname;
    std::lock_guard<std::mutex> lock(g_libs_mutex);
    DynLib & lib = g_loaded_libs[libname];
    if (lib.is_open() == false && lib.open(libname) == false)
    {
        g_loaded_libs.erase(libname);
        return nullptr;
    }
    constructor_sym = (sihd::util::Named * (*)(std::string, sihd::util::Node *)) lib.load(factory_name);
    if (constructor_sym == nullptr)
        return nullptr;
    return constructor_sym(name, parent);
}

bool PluginLoader::unload(const std::string & libname)
{
    std::lock_guard<std::mutex> lock(g_libs_mutex);
    return g_loaded_libs.erase(libname) > 0;
}

} // namespace sihd::sys
