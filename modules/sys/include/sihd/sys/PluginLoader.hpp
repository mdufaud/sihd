#ifndef __SIHD_SYS_PLUGINLOADER_HPP__
#define __SIHD_SYS_PLUGINLOADER_HPP__

#include <expected>
#include <string>

#include <sihd/util/Error.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::sys
{

class PluginLoader
{
    public:
        PluginLoader() = delete;
        ~PluginLoader() = delete;

        static std::expected<sihd::util::Named *, sihd::util::Error>
            create_from_library(const std::string & libname,
                                const std::string & classname,
                                const std::string & name,
                                sihd::util::Node *parent = nullptr);

        // libraries opened by create_from_library carry no RTLD_GLOBAL and are invisible here
        static std::expected<sihd::util::Named *, sihd::util::Error>
            create_from_linked(const std::string & classname,
                               const std::string & name,
                               sihd::util::Node *parent = nullptr);

        // unload a previously loaded library by name - returns false if not found
        static bool unload(const std::string & libname);
};

} // namespace sihd::sys

#endif
