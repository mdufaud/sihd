#ifndef __SIHD_SYS_INTERNAL_NT_DLL_HPP__
#define __SIHD_SYS_INTERNAL_NT_DLL_HPP__

#include <windows.h>

namespace sihd::sys::internal
{

// optional APIs absent from wine/old windows; GetModuleHandle only sees loaded modules (powrprof is not by default)
inline void *nt_dll_symbol(const char *dll, const char *symbol)
{
    HMODULE mod = GetModuleHandleA(dll);
    if (mod == nullptr)
        mod = LoadLibraryA(dll);
    return (void *)GetProcAddress(mod, symbol);
}

} // namespace sihd::sys::internal

#endif
