#include <iostream>
#include <string_view>

#include <fmt/core.h>

#include <sihd/lua.hpp>

using namespace sihd::util;
using namespace sihd::lua;

#define PROMPT "$> "

static void run_line(Vm & vm, std::string_view str)
{
    if (auto res = vm.do_string(str); !res)
        std::cerr << fmt::format("{} (code = {})\n", res.error().message, (int)res.error().code);
}

// Order matters: a derived class needs its base registered first.
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
    run_line(vm, "package.path = sihd.dir .. '/etc/sihd/lua/?.lua;' .. package.path");
    run_line(vm, "require 'luabin.preload'");
#endif

    std::string line;
    std::cout << PROMPT;
    while (std::getline(std::cin, line))
    {
        try
        {
            run_line(vm, line);
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
