#ifndef __SIHD_UTIL_NAMEDFACTORY_HPP__
#define __SIHD_UTIL_NAMEDFACTORY_HPP__

#include <sihd/util/Named.hpp>

namespace sihd::util
{

class NamedFactory
{
    public:
        NamedFactory() = delete;
        ~NamedFactory() = delete;

        // read dynamic library libname to search for classname and creates and returns it from name/parent
        static Named *load(const std::string & libname,
                           const std::string & classname,
                           const std::string & name,
                           Node *parent = nullptr);
};

} // namespace sihd::util

// creating a new known symbol to go around C++ class name mangling
#define SIHD_UTIL_REGISTER_FACTORY(class)                                                                              \
 extern "C"                                                                                                            \
 {                                                                                                                     \
 extern sihd::util::Named *sihd_util_namedfactory_##class(const std::string & name, sihd::util::Node *parent)          \
 {                                                                                                                     \
  return new class(name, parent);                                                                                      \
 }                                                                                                                     \
 };

#define SIHD_UTIL_FACTORY(class, name, parent) sihd_util_namedfactory_##class(name, parent);

#endif
