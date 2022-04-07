#include <sihd/lua/Vm.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::lua
{

SIHD_NEW_LOGGER("sihd::lua::api");

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

bool    Vm::ref_exists(std::string_view name)
{
    lua_getglobal(_state_ptr, name.data());
    return lua_isnil(_state_ptr, -1) == 0;
}

bool    Vm::refs_exists(const std::initializer_list<std::string_view> & lst)
{
    if (lst.empty())
        return false;
    size_t i = 0;
    while (i < lst.size())
    {
        if (i == 0)
        {
            // first time
            lua_getglobal(_state_ptr, lst[i].data());
        }
        else
        {
            if (lua_istable(_state_ptr, -1) == 0)
            {
                lua_remove(_state_ptr, -1);
                return false;
            }
            lua_getfield(_state_ptr, -1, lst[i].data());
            lua_remove(_state_ptr, -2);
        }
        ++i;
    }
    bool ret = lua_isnil(_state_ptr, -1) == 0;
    lua_remove(_state_ptr, -1);
    return ret;
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

void    Vm::print_stack(int max, FILE *output)
{
    fprintf(output, this->dump_stack(max).c_str());
}

std::string     Vm::dump_stack(int max)
{
    if (_state_ptr == nullptr)
        return "";
    std::stringstream ss;
    int top = lua_gettop(_state_ptr);
    if (max >= 0)
        top = std::min(top, max);
    ss << "lua stack " << _state_ptr << " (" << top << "):\n";
    int i = 1;
    while (i <= top)
    {
        ss << "stack[" << i << "]:\t";
        switch (lua_type(_state_ptr, i))
        {
            case LUA_TNUMBER:
                ss << lua_tonumber(_state_ptr, i);
                break ;
            case LUA_TSTRING:
                ss << lua_tostring(_state_ptr, i);
                break ;
            case LUA_TBOOLEAN:
                ss << (lua_toboolean(_state_ptr, i) ? "true" : "false");
                break ;
            case LUA_TNIL:
                ss << "nil";
                break ;
            default:
                ss << lua_topointer(_state_ptr, i);
                break ;
        }
        ss << " (" << luaL_typename(_state_ptr, i) << ")\n";
        ++i;
    }
    return ss.str();
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