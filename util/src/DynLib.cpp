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

bool try_load_lib(std::string && lib_name, void **handle, std::string & fill)
{
#if !defined(__SIHD_WINDOWS__)
    *handle = dlopen(lib_name.c_str(), RTLD_NOW);
#else
      std::wstring w_lib_name{lib_name.begin(), lib_name.end()};
      *handle = LoadLibrary(w_lib_name.c_str());
#endif
    if (*handle != nullptr)
        fill = std::move(lib_name);
    return *handle != nullptr;
}

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

#if !defined(__SIHD_WINDOWS__)
    try_load_lib(fmt::format("lib{}.so", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}.so", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}", lib_name), &_handle, _name);
#else
    try_load_lib(fmt::format("lib{}.dll", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}.dll", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}", lib_name), &_handle, _name);
#endif
    if (_handle == nullptr)
        SIHD_LOG(error, "DynLib: {}", dlerror());
    return _handle != nullptr;
}

void *DynLib::load(std::string_view symbol_name)
{
    void *ret = nullptr;

    if (this->is_open())
    {
#if !defined(__SIHD_WINDOWS__)
        ret = dlsym(_handle, symbol_name.data());
        if (ret == nullptr)
            SIHD_LOG(error, "DynLib: {}", dlerror());
#else
        ret = GetProcAddress(_handle, symbol_name.data());
        if (ret == nullptr)
            SIHD_LOG(error, "DynLib: could not load symbol {}", symbol_name);
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
#else
        FreeLibrary(_handle);
#endif
        _handle = nullptr;
        _name.clear();
    }
    return ret;
}

} // namespace sihd::util
