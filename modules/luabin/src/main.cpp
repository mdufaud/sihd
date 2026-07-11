#include <iostream>

#include <sihd/lua.hpp>

using namespace sihd::util;
using namespace sihd::lua;

#define PROMPT "$> "

// Register every binding this build actually compiled. lua only compiles the
// binding of a module that is in the build, so each api is guarded by its define
// from the generated config header. Order matters: a derived class needs its base
// registered first (net/http derive core's Device and util's Node).
static void load_available_apis(Vm & vm)
{
#if SIHD_LUA_WITH_UTIL
    LuaUtilApi::load_all(vm);
#endif
#if SIHD_LUA_WITH_SYS
    LuaSysApi::load_all(vm);
#endif
#if SIHD_LUA_WITH_JSON
    LuaJsonApi::load_all(vm);
#endif
#if SIHD_LUA_WITH_CSV
    LuaCsvApi::load_all(vm);
#endif
#if SIHD_LUA_WITH_ZIP
    LuaZipApi::load_all(vm);
#endif
#if SIHD_LUA_WITH_CORE
    LuaCoreApi::load(vm);
#endif
#if SIHD_LUA_WITH_NET
    LuaNetApi::load_all(vm);
#endif
#if SIHD_LUA_WITH_HTTP
    LuaHttpApi::load_all(vm);
#endif
}

int main(void)
{
    Vm vm;

    load_available_apis(vm);

#if SIHD_LUA_WITH_SYS
    // sihd.dir is registered by the sys binding
    vm.do_string("package.path = sihd.dir .. '/etc/sihd/lua/?.lua;' .. package.path");
    vm.do_string("require 'luabin.preload'");
#endif

    std::string line;
    std::cout << PROMPT;
    while (std::getline(std::cin, line))
    {
        try
        {
            vm.do_string(line);
        }
        catch (const std::exception & e)
        {
            std::cerr << e.what() << std::endl;
        }
        std::cout << PROMPT;
    }
    std::cout << std::endl;
    return 0;
}
