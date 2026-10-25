#include <errhandlingapi.h>
#include <libloaderapi.h>
#include <windows.h>

#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Result.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

static_assert(DynLib::supported);

namespace
{

// GetLastError codes share mingw errno numbers (ERROR_MOD_NOT_FOUND == ECONNRESET): error_errno misclassifies
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

} // namespace

Result<void> DynLib::open(ZStringView file)
{
    this->close();
    _handle = LoadLibraryEx(file.c_str(), nullptr, 0);
    if (_handle == nullptr)
        return lib_error();
    return {};
}

Result<void> DynLib::open_system(ZStringView name)
{
    // a drive prefix (C:name.dll) is relative to that drive's current directory
    if (name.view().find_first_of("/\\:") != std::string::npos)
        return std::unexpected(Error(invalid_argument, "not a library name: '{}'", name));
    this->close();
    _handle = LoadLibraryEx(name.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (_handle == nullptr)
        return lib_error();
    return {};
}

Result<void *> DynLib::load_symbol(ZStringView symbol_name) const
{
    if (this->is_open() == false)
        return std::unexpected(Error(not_initialized, "no library open"));
    void *symbol = (void *)GetProcAddress((HMODULE)_handle, symbol_name.c_str());
    if (symbol == nullptr)
        return lib_error();
    return symbol;
}

bool DynLib::close()
{
    if (this->is_open() == false)
        return true;
    const bool closed = FreeLibrary((HMODULE)_handle) != 0;
    _handle = nullptr;
    return closed;
}

} // namespace sihd::sys
