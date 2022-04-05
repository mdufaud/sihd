#include <sihd/lua/Vm.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::lua
{

SIHD_NEW_LOGGER("sihd::luapi");

std::map<lua_State *, Vm *> Vm::_map_vm;

Vm::Vm(): _state_ownership(false), _state_ptr(nullptr)
{
    this->new_state();
}

Vm::Vm(lua_State *state): _state_ownership(false), _state_ptr(nullptr)
{
    this->set_state(state);
}

Vm::~Vm()
{
    this->close_state();
}

void    Vm::set_state(lua_State *state)
{
    this->close_state();
    _state_ownership = false;
    _state_ptr = state;
    if (state != nullptr)
        Vm::_register_vm(state, this);
}

void    Vm::close_state()
{
    if (_state_ptr != nullptr)
    {
        Vm::_unregister_vm(_state_ptr);
        if (_state_ownership)
            lua_close(_state_ptr);
    }
    _state_ptr = nullptr;
}

bool    Vm::new_state()
{
    this->close_state();
    _state_ptr = luaL_newstate();
    if (_state_ptr != nullptr)
    {
        Vm::_register_vm(_state_ptr, this);
        _state_ownership = true;
        luaL_openlibs(_state_ptr);
    }
    return _state_ptr != nullptr;
}

bool    Vm::new_thread(Vm & vm)
{
    lua_State *thread_state = lua_newthread(_state_ptr);
    if (thread_state != nullptr)
        vm.set_state(thread_state);
    return thread_state != nullptr;
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

void    Vm::dump_stack(int max, FILE *output)
{
    if (_state_ptr == nullptr)
        return ;
    int top = lua_gettop(_state_ptr);
    if (max >= 0)
        top = std::min(top, max);
    fprintf(output, "lua stack %p (%d):\n", _state_ptr, top);
    int i = 1;
    while (i <= top)
    {
        fprintf(output, "stack[%d]:\t", i);
        switch (lua_type(_state_ptr, i))
        {
            case LUA_TNUMBER:
                fprintf(output, "%g", lua_tonumber(_state_ptr, i));
                break ;
            case LUA_TSTRING:
                fprintf(output, "%s", lua_tostring(_state_ptr, i));
                break ;
            case LUA_TBOOLEAN:
                fprintf(output, "%s", (lua_toboolean(_state_ptr, i) ? "true" : "false"));
                break ;
            case LUA_TNIL:
                fprintf(output, "nil");
                break ;
            default:
                fprintf(output, "%p", lua_topointer(_state_ptr, i));
                break ;
        }
        fprintf(output, " (%s)\n", luaL_typename(_state_ptr, i));
        ++i;
    }
}

/* ************************************************************************* */
/* Vm static methods */
/* ************************************************************************* */

Vm  *Vm::get_vm(lua_State *state)
{
    auto it = Vm::_map_vm.find(state);
    if (it != Vm::_map_vm.end())
        return it->second;
    return nullptr;
}

void    Vm::_register_vm(lua_State *state, Vm *vm)
{
    Vm::_map_vm[state] = vm;
}

void    Vm::_unregister_vm(lua_State *state)
{
    Vm::_map_vm.erase(state);
}

}