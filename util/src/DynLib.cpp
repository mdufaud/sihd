#include <sihd/util/platform.hpp>

#if defined(__SIHD_LINUX__)
# include <dlfcn.h>
#endif
#if defined(__SIHD_WINDOWS__)
# include <errhandlingapi.h>
# include <libloaderapi.h>
# include <windows.h>
#endif

#include <sihd/util/DynLib.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

namespace
{

std::string get_error()
{
#if !defined(SIHD_STATIC)
# if !defined(__SIHD_WINDOWS__)
    return dlerror();
# else
    // Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
    {
        return std::string(); // No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet
    // know how long the message string will be).
    size_t size
        = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                         NULL,
                         errorMessageID,
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                         (LPSTR)&messageBuffer,
                         0,
                         NULL);

    // Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
# endif
#else
    return "";
#endif
}

#if !defined(SIHD_STATIC)
bool try_load_lib(std::string && lib_name, void **handle, std::string & fill)
{
# if !defined(__SIHD_WINDOWS__)
    *handle = dlopen(lib_name.c_str(), RTLD_NOW);
# else
    *handle = LoadLibrary(lib_name.c_str());
# endif
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
#if !defined(SIHD_STATIC)
    this->close();
    std::string test_lib_name;

# if !defined(__SIHD_WINDOWS__)
    try_load_lib(fmt::format("lib{}.so", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}.so", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}", lib_name), &_handle, _name);
# else
    try_load_lib(fmt::format("lib{}.dll", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}.dll", lib_name), &_handle, _name)
        || try_load_lib(fmt::format("{}", lib_name), &_handle, _name);
# endif
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
#if !defined(__SIHD_WINDOWS__)
        ret = dlsym(_handle, symbol_name.data());
#else
        ret = (void *)GetProcAddress((HMODULE)_handle, symbol_name.data());
#endif
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
#if !defined(__SIHD_WINDOWS__)
        ret = dlclose(_handle) == 0;
#else
        ret = FreeLibrary((HMODULE)_handle);
#endif
        if (ret == false)
            SIHD_LOG(error, "DynLib: {}", get_error());
        _handle = nullptr;
        _name.clear();
    }
    return ret;
}

} // namespace sihd::util
