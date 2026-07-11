#ifndef __SIHD_LUA_LUAZIPAPI_HPP__
#define __SIHD_LUA_LUAZIPAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

namespace sihd::lua
{

class LuaZipApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaZipApi() = delete;
        ~LuaZipApi() = delete;
};

} // namespace sihd::lua

#endif
