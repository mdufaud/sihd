#include <iostream>
#include <sihd/lua/util/LuaUtilApi.hpp>
// #include <sol/sol.hpp>

using namespace sihd::util;
using namespace sihd::lua;

#define PROMPT "$> "

int main(void)
{
    LoggerManager::basic();

    // sol::state lua;
    // // For print etc...
    // lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::os, sol::lib::string);
    // sihd::lua::LuaUtilApi::load(lua);

    // lua.script("package.path = sihd.dir .. '/etc/sihd/lua/?.lua;' .. package.path");
    // lua.script("require 'luabin.preload'");

    // std::string line;
    // std::cout << PROMPT;
    // while (std::getline(std::cin, line))
    // {
    //     try
    //     {
    //         lua.script(line);
    //     }
    //     catch (const std::exception & e)
    //     {
    //         LuaUtilApi::logger.log(error, e.what());
    //     }
    //     std::cout << PROMPT;
    // }
    // std::cout << std::endl;
    return 0;
}
