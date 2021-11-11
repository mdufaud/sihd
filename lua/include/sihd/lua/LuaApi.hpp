#ifndef __SIHD_LUA_LUAAPI_HPP__
# define __SIHD_LUA_LUAAPI_HPP__

# include <sol/sol.hpp>

namespace sihd::lua
{

class LuaApi
{
    public:
        static void load(sol::state & state);

    private:
        LuaApi() {};
        virtual ~LuaApi() {};
};

}

#endif