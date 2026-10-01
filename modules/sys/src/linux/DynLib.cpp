#include <sihd/sys/platform.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <dlfcn.h>
#endif

#include <iterator>

#include <sihd/sys/DynLib.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
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

#if !defined(SIHD_STATIC) && !defined(__SIHD_EMSCRIPTEN__)

std::unexpected<Error> lib_error(std::string message)
{
    // dlerror embeds the strerror text of the cause; a localized libc classifies as io_error
    const bool missing = message.find("No such file or directory") != std::string::npos;
    return std::unexpected(Error(missing ? not_found : io_error, std::move(message)));
}

bool try_load_lib(std::string && lib_name, void **handle, std::string & fill)
{
    *handle = dlopen(lib_name.c_str(), RTLD_NOW);
    if (*handle != nullptr)
        fill = std::move(lib_name);
    return *handle != nullptr;
}
#endif

} // namespace

SIHD_LOGGER;

std::expected<void, Error> DynLib::open([[maybe_unused]] std::string_view lib_name)
{
#if !defined(SIHD_STATIC) && !defined(__SIHD_EMSCRIPTEN__)
    this->close();
    constexpr std::string_view patterns[] = {"lib{}.so", "{}.so", "{}"};
    for (size_t i = 0; i < std::size(patterns); ++i)
    {
        if (try_load_lib(fmt::format(fmt::runtime(patterns[i]), lib_name), &_handle, _name))
            return {};
        if (i + 1 < std::size(patterns))
            SIHD_LOG(debug, "DynLib: {}", get_error());
    }
    return lib_error(get_error());
#else
    return std::unexpected(Error(not_supported, "dynamic loading unavailable in this build"));
#endif
}

std::expected<void *, Error> DynLib::load([[maybe_unused]] std::string_view symbol_name)
{
#if !defined(SIHD_STATIC) && !defined(__SIHD_EMSCRIPTEN__)
    if (this->is_open() == false)
        return std::unexpected(Error(not_initialized, "no library open"));
    void *ret = dlsym(_handle, symbol_name.data());
    if (ret == nullptr)
        return std::unexpected(Error(not_found, get_error()));
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
#if !defined(__SIHD_EMSCRIPTEN__)
        ret = dlclose(_handle) == 0;
#endif
        if (ret == false)
            SIHD_LOG(error, "DynLib: {}", get_error());
        _handle = nullptr;
        _name.clear();
    }
    return ret;
}

} // namespace sihd::sys
