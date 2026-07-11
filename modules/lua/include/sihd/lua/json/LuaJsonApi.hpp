#ifndef __SIHD_LUA_LUAJSONAPI_HPP__
#define __SIHD_LUA_LUAJSONAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

namespace sihd::lua
{

class LuaJsonApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaJsonApi() = delete;
        ~LuaJsonApi() = delete;
};

} // namespace sihd::lua

#endif
