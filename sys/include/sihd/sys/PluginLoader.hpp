#ifndef __SIHD_SYS_PLUGINLOADER_HPP__
#define __SIHD_SYS_PLUGINLOADER_HPP__

#include <string>

#include <sihd/util/Named.hpp>

namespace sihd::sys
{

class PluginLoader
{
    public:
        PluginLoader() = delete;
        ~PluginLoader() = delete;

        // read dynamic library libname to search for classname and creates and returns it from name/parent
        static sihd::util::Named *load(const std::string & libname,
                                       const std::string & classname,
                                       const std::string & name,
                                       sihd::util::Node *parent = nullptr);
};

} // namespace sihd::sys

#endif
