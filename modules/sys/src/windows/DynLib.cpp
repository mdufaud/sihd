#include <errhandlingapi.h>
#include <libloaderapi.h>
#include <windows.h>

#include <iterator>

#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

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

// GetLastError codes share mingw errno numbers (ERROR_MOD_NOT_FOUND == ECONNRESET):
// error_errno cannot classify them, the errno meaning would win
bool is_not_found_error(int code)
{
    return code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND || code == ERROR_MOD_NOT_FOUND
           || code == ERROR_DLL_NOT_FOUND || code == ERROR_PROC_NOT_FOUND;
}

std::unexpected<Error> lib_error()
{
    const int code = os::last_error();
    return std::unexpected(Error(is_not_found_error(code) ? not_found : io_error, os::error_str(code)));
}

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

std::expected<void, Error> DynLib::open([[maybe_unused]] std::string_view lib_name)
{
#if !defined(SIHD_STATIC)
    this->close();
    constexpr std::string_view patterns[] = {"lib{}.dll", "{}.dll", "{}"};
    for (size_t i = 0; i < std::size(patterns); ++i)
    {
        if (try_load_lib(fmt::format(fmt::runtime(patterns[i]), lib_name), &_handle, _name))
            return {};
        if (i + 1 < std::size(patterns))
            SIHD_LOG(debug, "DynLib: {}", get_error());
    }
    return lib_error();
#else
    return std::unexpected(Error(not_supported, "dynamic loading unavailable in this build"));
#endif
}

std::expected<void *, Error> DynLib::load([[maybe_unused]] std::string_view symbol_name)
{
#if !defined(SIHD_STATIC)
    if (this->is_open() == false)
        return std::unexpected(Error(not_initialized, "no library open"));
    void *ret = (void *)GetProcAddress((HMODULE)_handle, symbol_name.data());
    if (ret == nullptr)
        return lib_error();
    return ret;
#else
    return std::unexpected(Error(not_supported, "dynamic loading unavailable in this build"));
#endif
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
