#ifndef __SIHD_SYS_PLUGINLOADER_HPP__
#define __SIHD_SYS_PLUGINLOADER_HPP__

#include <string>

#include <sihd/util/Named.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys
{

class PluginLoader
{
    public:
        PluginLoader() = delete;
        ~PluginLoader() = delete;

        static sihd::util::Result<sihd::util::Named *> create_from_library(const std::string & libname,
                                                                           const std::string & classname,
                                                                           const std::string & name,
                                                                           sihd::util::Node *parent = nullptr);

        // libraries opened by create_from_library carry no RTLD_GLOBAL and are invisible here
        static sihd::util::Result<sihd::util::Named *> create_from_linked(const std::string & classname,
                                                                          const std::string & name,
                                                                          sihd::util::Node *parent = nullptr);

        static bool unload(const std::string & libname);
};

} // namespace sihd::sys

#endif
