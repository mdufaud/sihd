#include <cstring>

#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/thread.hpp>

#if defined(__SIHD_LINUX__)
# include <sys/prctl.h>
#elif defined(__SIHD_WINDOWS__)
# include <windows.h>
// # include <processthreadsapi.h>
#endif

namespace sihd::util::thread
{

namespace
{

pthread_t do_init()
{
#if defined(__SIHD_WINDOWS__)
    set_name("main");
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
    return "0x" + str::to_hex(id);
}

void set_name(const std::string & name)
{
#if defined(__SIHD_LINUX__)
    const std::string subname = name.substr(0, 15);
    if (prctl(PR_SET_NAME, subname.c_str()) != 0)
    {
        throw std::runtime_error("Failed to set thread name");
    }
    l_thread_name = subname;
#elif defined(__SIHD_WINDOWS__)
    using _SetThreadDescription = HRESULT(WINAPI *)(HANDLE, PCWSTR);
    void *ptr = (void *)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "SetThreadDescription");
    if (ptr == nullptr)
    {
        throw std::runtime_error("SetThreadDescription not found");
    }
    _SetThreadDescription set_thread_description = reinterpret_cast<_SetThreadDescription>(ptr);
    if (set_thread_description == nullptr)
    {
        throw std::runtime_error("SetThreadDescription not found");
    }
    HRESULT hr = set_thread_description(GetCurrentThread(), str::to_wstr(name).c_str());
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to set thread name");
    }
    l_thread_name = name;
#endif
}

const std::string & name()
{
#if defined(__SIHD_LINUX__)
    if (l_thread_name.empty())
    {
        l_thread_name.resize(16);
        if (prctl(PR_GET_NAME, l_thread_name.data()) != 0)
        {
            throw std::runtime_error("Failed to get thread name");
        }
        l_thread_name.resize(strlen(l_thread_name.c_str()));
    }
#elif defined(__SIHD_WINDOWS__)
    using _GetThreadDescription = HRESULT(WINAPI *)(HANDLE, PWSTR *);
    if (l_thread_name.empty())
    {
        void *ptr = (void *)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetThreadDescription");
        if (ptr == nullptr)
        {
            throw std::runtime_error("GetThreadDescription not found");
        }
        _GetThreadDescription get_thread_description = reinterpret_cast<_GetThreadDescription>(ptr);
        if (get_thread_description == nullptr)
        {
            throw std::runtime_error("GetThreadDescription not found");
        }
        PWSTR wname = nullptr;
        HRESULT hr = get_thread_description(GetCurrentThread(), &wname);
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to get thread name");
        }
        l_thread_name = str::to_str(wname);
        LocalFree(wname);
    }
#endif
    return l_thread_name;
    ;
}

} // namespace sihd::util::thread
