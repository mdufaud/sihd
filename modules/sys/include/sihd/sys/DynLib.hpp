#ifndef __SIHD_SYS_DYNLIB_HPP__
#define __SIHD_SYS_DYNLIB_HPP__

#include <string>
#include <type_traits>

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

class DynLib
{
    public:
        // false where the libc has no usable loader: emscripten, static linux (musl stub, glibc second copy)
        static constexpr bool supported = !sihd::util::build::is_emscripten
                                          && (sihd::util::build::is_windows
                                              || !sihd::util::build::is_statically_linked);

        DynLib();
        explicit DynLib(const std::string & file);
        ~DynLib();

        DynLib(const DynLib &) = delete;
        DynLib & operator=(const DynLib &) = delete;

        sihd::util::Result<void> open(const std::string & file);

        // refuses paths: linux searches the loader path, windows System32 alone
        sihd::util::Result<void> open_system(const std::string & name);

        sihd::util::Result<void *> load_symbol(const std::string & symbol_name) const;

        template <typename Function>
            requires std::is_function_v<Function>
        sihd::util::Result<Function *> load_function(const std::string & name) const
        {
            return this->load_symbol(name).transform([](void *symbol) { return reinterpret_cast<Function *>(symbol); });
        }

        bool close();

        bool is_open() const { return _handle != nullptr; }

    private:
        void *_handle;
};

} // namespace sihd::sys

#endif
