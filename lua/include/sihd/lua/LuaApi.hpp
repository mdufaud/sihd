#ifndef __SIHD_LUA_LUAAPI_HPP__
# define __SIHD_LUA_LUAAPI_HPP__

extern "C"
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

# include <LuaBridge/LuaBridge.h>
# include <string>

namespace sihd::lua
{

class LuaApi
{
    public:
        static void load(lua_State *state);

        static std::string dir;

    private:
        LuaApi() {};
        virtual ~LuaApi() {};

};

}

#endif