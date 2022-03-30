#include <sihd/lua/LuaApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Files.hpp>

namespace sihd::lua
{

SIHD_NEW_LOGGER("sihd::apilua");

using namespace sihd::util;

std::string LuaApi::dir = Files::get_parent(Files::get_parent(OS::get_executable_path()));

void    LuaApi::load(lua_State *state)
{
    luabridge::getGlobalNamespace(state)
        .beginNamespace("sihd")
            // from sihd_path/bin/exe.lua -> sihd[dir] = sihd_path
            .addProperty("dir", &LuaApi::dir, false)
        .endNamespace();
}

}