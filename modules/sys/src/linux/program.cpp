#include <sihd/sys/platform.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <dlfcn.h>
#endif

#include <filesystem>
#include <fstream>

#include <sihd/sys/program.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::program
{

namespace
{

std::string get_error()
{
#if !defined(SIHD_STATIC) && !defined(__SIHD_EMSCRIPTEN__)
    return dlerror();
#else
    return "";
#endif
}

} // namespace

std::string path()
{
#if defined(__SIHD_EMSCRIPTEN__)
    return "";
#else
    std::string exe;
    try
    {
        exe = std::filesystem::canonical("/proc/self/exe");
        if (exe.empty() == false)
            return exe;
    }
    catch ([[maybe_unused]] const std::filesystem::filesystem_error & e)
    {
    }
    std::ifstream mapf("/proc/self/maps");
    std::string line;
    if (std::getline(mapf, line))
    {
        size_t idx = line.find("/");
        if (idx != std::string::npos)
        {
            exe = line.substr(idx);
            return exe;
        }
    }
#endif
    return ".";
}

std::expected<void *, Error> find_symbol([[maybe_unused]] std::string_view symbol_name)
{
#if !defined(__SIHD_EMSCRIPTEN__)
    void *ret = dlsym(RTLD_DEFAULT, symbol_name.data());
    if (ret == nullptr)
        return std::unexpected(Error(not_found, get_error()));
    return ret;
#else
    return std::unexpected(Error(not_supported, "dynamic loading unavailable in this build"));
#endif
}

} // namespace sihd::sys::program
