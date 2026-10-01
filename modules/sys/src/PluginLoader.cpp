#include <mutex>
#include <unordered_map>

#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/sys/PluginLoader.hpp>
#include <sihd/util/Logger.hpp>

#define SIHD_FACTORY_PREFIX "sihd_factory_"

using enum sihd::util::ErrorCode;

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

std::mutex g_libs_mutex;
std::unordered_map<std::string, DynLib> g_loaded_libs;

} // namespace

std::expected<sihd::util::Named *, Error> PluginLoader::load(const std::string & libname,
                                                             const std::string & classname,
                                                             const std::string & name,
                                                             sihd::util::Node *parent)
{
    std::string factory_name = SIHD_FACTORY_PREFIX + classname;
    std::lock_guard<std::mutex> lock(g_libs_mutex);
    DynLib & lib = g_loaded_libs[libname];
    if (lib.is_open() == false)
    {
        if (auto opened = lib.open(libname); opened.has_value() == false)
        {
            g_loaded_libs.erase(libname);
            SIHD_UNEXPECTED_RETURN(opened);
        }
    }
    auto symbol = lib.load(factory_name);
    if (symbol.has_value() == false)
    {
        SIHD_UNEXPECTED_RETURN(symbol);
    }
    auto constructor_sym = (sihd::util::Named * (*)(std::string, sihd::util::Node *)) symbol.value();
    sihd::util::Named *created = constructor_sym(name, parent);
    if (created == nullptr)
    {
        return std::unexpected(Error(not_found, "factory '{}' of '{}' created no object", factory_name, libname));
    }
    return created;
}

bool PluginLoader::unload(const std::string & libname)
{
    std::lock_guard<std::mutex> lock(g_libs_mutex);
    if (g_loaded_libs.count(libname) > 0)
        SIHD_LOG(warning, "PluginLoader: unloading '{}' — objects created from this library become dangling", libname);
    return g_loaded_libs.erase(libname) > 0;
}

} // namespace sihd::sys
