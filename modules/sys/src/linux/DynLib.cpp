#include <sihd/sys/DynLib.hpp>

#if !defined(SIHD_STATIC) && !defined(__SIHD_EMSCRIPTEN__)
# include <dlfcn.h>
#endif

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

#if !defined(SIHD_STATIC) && !defined(__SIHD_EMSCRIPTEN__)

static_assert(DynLib::supported);

namespace
{

std::unexpected<Error> lib_error()
{
    std::string message = dlerror();
    // dlerror embeds the strerror text of the cause; a localized libc classifies as io_error
    const bool missing = message.find("No such file or directory") != std::string::npos;
    return std::unexpected(Error(missing ? not_found : io_error, std::move(message)));
}

} // namespace

Result<void> DynLib::open(const std::string & file)
{
    this->close();
    _handle = dlopen(file.c_str(), RTLD_NOW);
    if (_handle == nullptr)
        return lib_error();
    return {};
}

Result<void> DynLib::open_system(const std::string & name)
{
    if (name.find('/') != std::string::npos)
        return std::unexpected(Error(invalid_argument, "not a library name: '{}'", name));
    return this->open(name);
}

Result<void *> DynLib::load_symbol(const std::string & symbol_name) const
{
    if (this->is_open() == false)
        return std::unexpected(Error(not_initialized, "no library open"));
    dlerror();
    void *symbol = dlsym(_handle, symbol_name.c_str());
    if (symbol != nullptr)
        return symbol;
    const char *error = dlerror();
    if (error == nullptr)
        return std::unexpected(Error(not_found, "symbol '{}' resolves to a null address", symbol_name));
    return std::unexpected(Error(not_found, std::string(error)));
}

bool DynLib::close()
{
    if (this->is_open() == false)
        return true;
    const bool closed = dlclose(_handle) == 0;
    _handle = nullptr;
    return closed;
}

#else

static_assert(!DynLib::supported);

Result<void> DynLib::open([[maybe_unused]] const std::string & file)
{
    return std::unexpected(Error(not_supported, "dynamic loading unavailable in this build"));
}

Result<void> DynLib::open_system([[maybe_unused]] const std::string & name)
{
    return std::unexpected(Error(not_supported, "dynamic loading unavailable in this build"));
}

Result<void *> DynLib::load_symbol([[maybe_unused]] const std::string & symbol_name) const
{
    return std::unexpected(Error(not_supported, "dynamic loading unavailable in this build"));
}

bool DynLib::close()
{
    return true;
}

#endif

} // namespace sihd::sys
