#include <sihd/lua/LuaApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Files.hpp>

namespace sihd::lua
{

using namespace sihd::util;

void    LuaApi::load(sol::state & lua)
{
    if (lua.get_or("sihd", nullptr) != nullptr)
        return ;
    sol::table sihd = lua.create_named_table("sihd");
    // from sihd_path/bin/exe.lua -> sihd[dir] = sihd_path
    sihd["dir"] = Files::get_parent(Files::get_parent(OS::get_executable_path()));
}

}