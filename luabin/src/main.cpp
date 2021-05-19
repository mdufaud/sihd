#include <iostream>
#include <sihd/core/Named.hpp>
#include <sol/sol.hpp>

int     main(void)
{
    sihd::core::Named obj("test");
    sol::state lua;
    // For print etc...
    lua.open_libraries(sol::lib::base);
    lua.script("print('" + obj.get_name() + "')");
    return 0;
}