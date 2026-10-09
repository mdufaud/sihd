#ifndef __SIHD_UTIL_NAMEDFACTORY_HPP__
#define __SIHD_UTIL_NAMEDFACTORY_HPP__

#include <string>
#include <string_view>

#include <sihd/util/Named.hpp>

// the factory table needs a static binary whose linker lays the section — static ELF only:
// PE (mingw) provides no __start_/__stop_ for it and wasm has no dynamic loader to replace
#if defined(SIHD_STATIC) && defined(__ELF__) && !defined(__SIHD_EMSCRIPTEN__)
# define SIHD_FACTORY_TABLE 1
#else
# define SIHD_FACTORY_TABLE 0
#endif

namespace sihd::util
{

// resolves through the sihd_factories linker section, laid by static ELF builds only;
// nullptr when no factory for the class is linked in this binary
Named *create_from_factory_table(std::string_view class_name, const std::string & name, Node *parent);

struct FactoryEntry
{
        const char *class_name;
        Named *(*create)(const std::string & name, Node *parent);
};

} // namespace sihd::util

// creating a new known symbol to go around C++ class name mangling; static ELF builds also lay
// an entry in the sihd_factories section so their binaries resolve factories without the loader
#define SIHD_REGISTER_FACTORY(class)                                                                                   \
    extern "C"                                                                                                         \
    {                                                                                                                  \
    extern sihd::util::Named *sihd_factory_##class(const std::string & name, sihd::util::Node *parent)                 \
    {                                                                                                                  \
        return new class(name, parent);                                                                                \
    }                                                                                                                  \
    };                                                                                                                 \
    SIHD_REGISTER_FACTORY_ENTRY(class)

#if SIHD_FACTORY_TABLE
# define SIHD_REGISTER_FACTORY_ENTRY(class)                                                                            \
     static const sihd::util::FactoryEntry sihd_factory_entry_##class __attribute__((                                  \
         used,                                                                                                         \
         section("sihd_factories"))) = {#class, &sihd_factory_##class};
#else
# define SIHD_REGISTER_FACTORY_ENTRY(class)
#endif

#endif
