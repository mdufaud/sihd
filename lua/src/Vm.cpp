#include <sihd/lua/Vm.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::lua
{

Vm::Vm(): _state_ptr(luaL_newstate())
{
    if (_state_ptr != nullptr)
        luaL_openlibs(_state_ptr);
}

Vm::~Vm()
{
    if (_state_ptr != nullptr)
        lua_close(_state_ptr);
}


bool    Vm::new_state()
{
    if (_state_ptr != nullptr)
    {
        lua_close(_state_ptr);
        _state_ptr = nullptr;
    }
    _state_ptr = luaL_newstate();
    if (_state_ptr != nullptr)
        luaL_openlibs(_state_ptr);
    return _state_ptr != nullptr;
}

lua_State   *Vm::new_thread()
{
    lua_State *ret = lua_newthread(_state_ptr);
    lua_insert(_state_ptr, 1);
    return ret;
}

luabridge::LuaRef   Vm::new_table()
{
    return luabridge::newTable(_state_ptr);
}

luabridge::LuaRef   Vm::get_ref(std::string_view name)
{
    return luabridge::getGlobal(_state_ptr, name.data());
}

bool    Vm::do_file(std::string_view path)
{
    return luaL_dofile(_state_ptr, path.data()) == 0;
}

bool    Vm::do_string(std::string_view path)
{
    return luaL_dostring(_state_ptr, path.data()) == 0;
}

std::string Vm::last_string()
{
    return lua_tostring(_state_ptr, -1);
}

std::lock_guard<std::mutex> Vm::lock()
{
    return std::lock_guard(_mutex);
}


}