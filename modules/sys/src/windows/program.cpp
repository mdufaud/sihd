#include <libloaderapi.h>
#include <psapi.h>
#include <windows.h>

#include <vector>

#include <sihd/sys/os.hpp>
#include <sihd/sys/program.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::program
{

std::string path()
{
    char exe[MAX_PATH];
    if (GetModuleFileName(NULL, exe, MAX_PATH) != 0)
        return exe;
    return ".";
}

std::expected<void *, Error> find_symbol([[maybe_unused]] std::string_view symbol_name)
{
#if !defined(SIHD_STATIC)
    DWORD needed = 0;
    if (EnumProcessModules(GetCurrentProcess(), nullptr, 0, &needed) == FALSE)
        return std::unexpected(Error(io_error, os::last_error_str()));
    std::vector<HMODULE> modules(needed / sizeof(HMODULE), nullptr);
    if (EnumProcessModules(GetCurrentProcess(), modules.data(), needed, &needed) == FALSE)
        return std::unexpected(Error(io_error, os::last_error_str()));
    for (HMODULE module : modules)
    {
        if (module == nullptr)
            continue;
        void *ret = (void *)GetProcAddress(module, symbol_name.data());
        if (ret != nullptr)
            return ret;
    }
    return std::unexpected(Error(not_found, "symbol '{}' not found in the loaded modules", symbol_name));
#else
    return std::unexpected(Error(not_supported, "dynamic loading unavailable in this build"));
#endif
}

} // namespace sihd::sys::program
