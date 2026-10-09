#ifndef __SIHD_SYS_DYNLIB_HPP__
#define __SIHD_SYS_DYNLIB_HPP__

#include <expected>
#include <string>
#include <string_view>

#include <sihd/util/Error.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

class DynLib
{
    public:
        // dynamic loading (dlopen/LoadLibrary): unavailable in static builds and on emscripten
        static constexpr bool supported = !sihd::util::build::is_statically_linked && !sihd::util::build::is_emscripten;

        DynLib();
        DynLib(std::string_view lib_name);
        ~DynLib();

        std::expected<void, sihd::util::Error> open(std::string_view lib_name);

        std::expected<void *, sihd::util::Error> load_symbol(std::string_view symbol_name);

        bool close();

        bool is_open() const { return _handle != nullptr; }

    protected:

    private:
        void *_handle;
        std::string _name;
};

} // namespace sihd::sys

#endif
