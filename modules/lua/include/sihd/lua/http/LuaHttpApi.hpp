#ifndef __SIHD_LUA_LUAHTTPAPI_HPP__
#define __SIHD_LUA_LUAHTTPAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

namespace sihd::lua
{

class LuaHttpApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaHttpApi() = delete;
        ~LuaHttpApi() = delete;
};

} // namespace sihd::lua

#endif
