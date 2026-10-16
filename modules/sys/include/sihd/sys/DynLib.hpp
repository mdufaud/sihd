#ifndef __SIHD_SYS_DYNLIB_HPP__
#define __SIHD_SYS_DYNLIB_HPP__

#include <string>

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

class DynLib
{
    public:
        // dynamic loading (dlopen/LoadLibrary): unavailable in static builds and on emscripten
        static constexpr bool supported = !sihd::util::build::is_statically_linked && !sihd::util::build::is_emscripten;

        DynLib();
        DynLib(const std::string & lib_name);
        ~DynLib();

        DynLib(const DynLib &) = delete;
        DynLib & operator=(const DynLib &) = delete;

        sihd::util::Result<void> open(const std::string & lib_name);

        sihd::util::Result<void *> load_symbol(const std::string & symbol_name);

        bool close();

        bool is_open() const { return _handle != nullptr; }

    protected:

    private:
        void *_handle;
        std::string _name;
};

} // namespace sihd::sys

#endif
