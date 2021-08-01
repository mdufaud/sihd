#include <iostream>
#include <sihd/util/Named.hpp>
#include <sol/sol.hpp>
#include <sihd/lua/LuaUtilApi.hpp>

int     main(void)
{
    sihd::util::Named obj("test");
    sol::state lua;
    // For print etc...
    lua.open_libraries(sol::lib::base);
    sihd::lua::LuaUtilApi::load(lua);
    lua.script("print('" + obj.get_name() + "')");
    lua.script("local named = Named.new('hello', nil); print(named:get_name())");
    return 0;
}
