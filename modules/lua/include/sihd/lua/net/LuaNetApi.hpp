#ifndef __SIHD_LUA_LUANETAPI_HPP__
#define __SIHD_LUA_LUANETAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

namespace sihd::lua
{

class LuaNetApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaNetApi() = delete;
        ~LuaNetApi() = delete;
};

} // namespace sihd::lua

#endif
