#include <iostream>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/sys/LuaSysApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>

using namespace sihd::util;
using namespace sihd::lua;

#define PROMPT "$> "

int main(void)
{
    Vm vm;

    LuaUtilApi::load_base(vm);
    LuaSysApi::load_base(vm);

    vm.do_string("package.path = sihd.dir .. '/etc/sihd/lua/?.lua;' .. package.path");
    vm.do_string("require 'luabin.preload'");

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
