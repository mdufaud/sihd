#ifndef __SIHD_LUA_LUACSVAPI_HPP__
#define __SIHD_LUA_LUACSVAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

namespace sihd::lua
{

class LuaCsvApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaCsvApi() = delete;
        ~LuaCsvApi() = delete;
};

} // namespace sihd::lua

#endif
