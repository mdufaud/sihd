#include <sihd/util/platform.hpp>

#if defined(__SIHD_LINUX__)
# include <dlfcn.h>
#endif
#if defined(__SIHD_WINDOWS__)
#endif

#include <sihd/util/DynLib.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

namespace
{

#if !defined(STATIC) && !defined(__SIHD_WINDOWS__)
bool try_load_lib(std::string && lib_name, void **handle, std::string & fill)
{
    *handle = dlopen(lib_name.c_str(), RTLD_NOW);
    if (*handle != nullptr)
        fill = std::move(lib_name);
    return *handle != nullptr;
}
#endif

} // namespace

SIHD_LOGGER;

DynLib::DynLib(): _handle(nullptr) {}

DynLib::DynLib(std::string_view lib_name): DynLib()
{
    this->open(lib_name);
}

DynLib::~DynLib()
{
    this->close();
}

bool DynLib::open(std::string_view lib_name)
{
    this->close();
    std::string test_lib_name;

#if !defined(STATIC) && !defined(__SIHD_WINDOWS__)
    try_load_lib(fmt::format("lib{}.so", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}.so", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}", lib_name), &_handle, _name);
    if (_handle == nullptr)
        SIHD_LOG(error, "DynLib: {}", dlerror());
#else
# pragma message("TODO")
    (void)lib_name;
#endif
    return _handle != nullptr;
}

void *DynLib::load(std::string_view symbol_name)
{
    void *ret = nullptr;

    if (this->is_open())
    {
#if !defined(STATIC) && !defined(__SIHD_WINDOWS__)
        ret = dlsym(_handle, symbol_name.data());
        if (ret == nullptr)
            SIHD_LOG(error, "DynLib: {}", dlerror());
#else
        (void)symbol_name;
#endif
    }
    return ret;
}

bool DynLib::close()
{
    bool ret = true;

    if (this->is_open())
    {
#if !defined(STATIC) && !defined(__SIHD_WINDOWS__)
        ret = dlclose(_handle) == 0;
        if (ret == false)
            SIHD_LOG(error, "DynLib: {}", dlerror());
        _handle = nullptr;
        _name.clear();
#else
#endif
    }
    return ret;
}

} // namespace sihd::util
