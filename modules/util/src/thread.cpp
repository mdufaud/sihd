#include <cstring>

#include <sihd/util/str.hpp>
#include <sihd/util/thread.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <sys/prctl.h>
#elif defined(__SIHD_WINDOWS__)
# include <windows.h>
// # include <processthreadsapi.h>
#endif

using enum sihd::util::ErrorCode;

namespace sihd::util::thread
{

namespace
{

pthread_t do_init()
{
#if defined(__SIHD_WINDOWS__)
    // static init: the logger is not up yet, a failed naming stays silent
    (void)set_name("main");
#endif
    return pthread_self();
}

pthread_t g_main_thread_id = do_init();
thread_local std::string l_thread_name;

} // namespace

pthread_t main()
{
    return g_main_thread_id;
}

bool equals(const pthread_t & id1, const pthread_t & id2)
{
    return memcmp(&id1, &id2, sizeof(pthread_t)) == 0;
}

pthread_t id()
{
    return pthread_self();
}

std::string id_str(pthread_t id)
{
    // pthread_t is opaque: an integer on glibc/musl/mingw, a pointer under Fil-C.
    static_assert(sizeof(pthread_t) <= sizeof(uint64_t));
    uint64_t value = 0;
    memcpy(&value, &id, sizeof(id));
    return "0x" + str::to_hex(value);
}

std::expected<void, Error> set_name(const std::string & name)
{
#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
    const std::string subname = name.substr(0, 15);
    if (prctl(PR_SET_NAME, subname.c_str()) != 0)
        return std::unexpected(Error::from_errno("could not set thread name '{}'", name));
    l_thread_name = subname;
#elif defined(__SIHD_WINDOWS__)
    using _SetThreadDescription = HRESULT(WINAPI *)(HANDLE, PCWSTR);
    void *ptr = (void *)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "SetThreadDescription");
    if (ptr == nullptr)
        return std::unexpected(Error(not_supported, "SetThreadDescription is missing"));
    auto set_thread_description = reinterpret_cast<_SetThreadDescription>(ptr);
    HRESULT hr = set_thread_description(GetCurrentThread(), str::to_wstr(name).c_str());
    if (FAILED(hr))
        return std::unexpected(Error(io_error, "could not set thread name '{}' (hr={:#x})", name, (unsigned long)hr));
    l_thread_name = name;
#else
    l_thread_name = name;
#endif
    return {};
}

const std::string & name()
{
#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
    if (l_thread_name.empty())
    {
        char buf[16] = {};
        if (prctl(PR_GET_NAME, buf) == 0)
            l_thread_name = buf;
    }
#elif defined(__SIHD_WINDOWS__)
    using _GetThreadDescription = HRESULT(WINAPI *)(HANDLE, PWSTR *);
    if (l_thread_name.empty())
    {
        void *ptr = (void *)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetThreadDescription");
        if (ptr != nullptr)
        {
            auto get_thread_description = reinterpret_cast<_GetThreadDescription>(ptr);
            PWSTR wname = nullptr;
            if (SUCCEEDED(get_thread_description(GetCurrentThread(), &wname)))
            {
                l_thread_name = str::to_str(wname);
                LocalFree(wname);
            }
        }
    }
#endif
    return l_thread_name;
}

} // namespace sihd::util::thread
