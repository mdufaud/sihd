#include <sihd/util/NamedFactory.hpp>

#if defined(__ELF__) && !defined(__SIHD_EMSCRIPTEN__)
// weak: shared flavors carry no sihd_factories section, the boundaries resolve to null there —
// and bind to a static-flavored binary's table when both are linked together
extern "C" const sihd::util::FactoryEntry __attribute__((weak)) __start_sihd_factories[];
extern "C" const sihd::util::FactoryEntry __attribute__((weak)) __stop_sihd_factories[];
#endif

namespace sihd::util
{

Named *create_from_factory_table(std::string_view class_name, const std::string & name, Node *parent)
{
#if defined(__ELF__) && !defined(__SIHD_EMSCRIPTEN__)
    for (const FactoryEntry *entry = __start_sihd_factories; entry != __stop_sihd_factories; ++entry)
    {
        if (class_name == entry->class_name)
            return entry->create(name, parent);
    }
#else
    (void)class_name;
    (void)name;
    (void)parent;
#endif
    return nullptr;
}

} // namespace sihd::util
