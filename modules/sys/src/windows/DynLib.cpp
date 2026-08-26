#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

#include <errhandlingapi.h>
#include <libloaderapi.h>
#include <windows.h>

namespace sihd::sys
{

using namespace sihd::util;

namespace
{

std::string get_error()
{
#if !defined(SIHD_STATIC)
    return os::last_error_str();
#else
    return "";
#endif
}

#if !defined(SIHD_STATIC)

bool try_load_lib(std::string && lib_name, void **handle, std::string & fill)
{
    *handle = LoadLibrary(lib_name.c_str());
    if (*handle != nullptr)
        fill = std::move(lib_name);
    return *handle != nullptr;
}

#endif

} // namespace

SIHD_LOGGER;

bool DynLib::open(std::string_view lib_name)
{
#if !defined(SIHD_STATIC)
    this->close();
    std::string test_lib_name;

    try_load_lib(fmt::format("lib{}.dll", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}.dll", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}", lib_name), &_handle, _name);
    if (_handle == nullptr)
        SIHD_LOG(error, "DynLib: {}", get_error());
    return _handle != nullptr;
#else
    (void)lib_name;
    return false;
#endif
}

void *DynLib::load(std::string_view symbol_name)
{
    void *ret = nullptr;

    if (this->is_open())
    {
        ret = (void *)GetProcAddress((HMODULE)_handle, symbol_name.data());
        if (ret == nullptr)
            SIHD_LOG(error, "DynLib: {}", get_error());
    }
    return ret;
}

bool DynLib::close()
{
    bool ret = true;

    if (this->is_open())
    {
        ret = FreeLibrary((HMODULE)_handle);
        if (ret == false)
            SIHD_LOG(error, "DynLib: {}", get_error());
        _handle = nullptr;
        _name.clear();
    }
    return ret;
}

} // namespace sihd::sys
