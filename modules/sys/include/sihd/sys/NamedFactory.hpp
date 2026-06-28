#ifndef __SIHD_SYS_NAMEDFACTORY_HPP__
#define __SIHD_SYS_NAMEDFACTORY_HPP__

#include <sihd/util/Named.hpp>

// creating a new known symbol to go around C++ class name mangling
#define SIHD_REGISTER_FACTORY(class)                                                                         \
    extern "C"                                                                                               \
    {                                                                                                        \
    extern sihd::util::Named *sihd_factory_##class(const std::string & name, sihd::util::Node *parent)       \
    {                                                                                                        \
        return new class(name, parent);                                                                      \
    }                                                                                                        \
    };

#define SIHD_FACTORY(class, name, parent) sihd_factory_##class(name, parent);

#endif
