#include <fmt/format.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::lua
{

SIHD_NEW_LOGGER("sihd::lua::api");

namespace
{

std::mutex g_mutex;
std::map<lua_State *, Vm *> g_map_vm;

void register_vm(lua_State *state, Vm *vm)
{
    std::lock_guard l(g_mutex);
    g_map_vm[state] = vm;
}

void unregister_vm(lua_State *state)
{
    std::lock_guard l(g_mutex);
    g_map_vm.erase(state);
}

} // namespace

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

void Vm::set_state(lua_State *state)
{
    this->close_state();
    _state_ownership = false;
    _state_ptr = state;
    if (state != nullptr)
        register_vm(state, this);
}

void Vm::close_state()
{
    if (_state_ptr != nullptr)
    {
        unregister_vm(_state_ptr);
        if (_state_ownership)
            lua_close(_state_ptr);
    }
    _state_ptr = nullptr;
}

bool Vm::new_state()
{
    this->close_state();
    _state_ptr = luaL_newstate();
    if (_state_ptr != nullptr)
    {
        register_vm(_state_ptr, this);
        _state_ownership = true;
        luaL_openlibs(_state_ptr);
    }
    return _state_ptr != nullptr;
}

lua_State *Vm::new_luathread()
{
    return lua_newthread(_state_ptr);
}

bool Vm::new_thread(Vm & vm)
{
    lua_State *thread_state = this->new_luathread();
    if (thread_state != nullptr)
        vm.set_state(thread_state);
    return thread_state != nullptr;
}

luabridge::LuaRef Vm::new_table()
{
    return luabridge::newTable(_state_ptr);
}

luabridge::LuaRef Vm::get_ref(std::string_view name)
{
    return luabridge::getGlobal(_state_ptr, name.data());
}

bool Vm::ref_exists(std::string_view name)
{
    lua_getglobal(_state_ptr, name.data());
    const bool exists = lua_isnil(_state_ptr, -1) == 0;
    lua_pop(_state_ptr, 1);
    return exists;
}

bool Vm::refs_exists(const std::initializer_list<std::string_view> & lst)
{
    if (lst.size() == 0)
        return false;
    bool first = true;
    for (const std::string_view & str : lst)
    {
        if (first)
        {
            lua_getglobal(_state_ptr, str.data());
            first = false;
        }
        else
        {
            if (lua_istable(_state_ptr, -1) == 0)
            {
                lua_remove(_state_ptr, -1);
                return false;
            }
            lua_getfield(_state_ptr, -1, str.data());
            lua_remove(_state_ptr, -2);
        }
    }
    bool ret = lua_isnil(_state_ptr, -1) == 0;
    lua_remove(_state_ptr, -1);
    return ret;
}

bool Vm::do_file(std::string_view path)
{
    return luaL_dofile(_state_ptr, path.data()) == 0;
}

bool Vm::do_string(std::string_view path)
{
    return luaL_dostring(_state_ptr, path.data()) == 0;
}

std::string Vm::last_string()
{
    const char *ret = lua_tostring(_state_ptr, -1);
    return ret == nullptr ? std::string() : std::string(ret);
}

void Vm::print_stack(int max, FILE *output)
{
    fprintf(output, "%s", this->dump_stack(max).c_str());
}

std::string Vm::dump_stack(int max)
{
    if (_state_ptr == nullptr)
        return "";
    std::string s;
    int top = lua_gettop(_state_ptr);
    if (max >= 0)
        top = std::min(top, max);
    s = fmt::format("lua stack {} ({}):\n", static_cast<void *>(_state_ptr), top);
    int i = 1;
    while (i <= top)
    {
        s += fmt::format("stack[{}]:\t", i);
        switch (lua_type(_state_ptr, i))
        {
            case LUA_TNUMBER:
                s += fmt::format("{}", lua_tonumber(_state_ptr, i));
                break;
            case LUA_TSTRING:
                s += fmt::format("{}", lua_tostring(_state_ptr, i));
                break;
            case LUA_TBOOLEAN:
                s += fmt::format("{}", (lua_toboolean(_state_ptr, i) ? "true" : "false"));
                break;
            case LUA_TNIL:
                s += "nil";
                break;
            default:
                s += fmt::format("{}", lua_topointer(_state_ptr, i));
                break;
        }
        s += fmt::format(" ({})\n", luaL_typename(_state_ptr, i));
        ++i;
    }
    return s;
}

/* ************************************************************************* */
/* Vm static methods */
/* ************************************************************************* */

Vm *Vm::get_vm(lua_State *state)
{
    std::lock_guard l(g_mutex);
    auto it = g_map_vm.find(state);
    if (it != g_map_vm.end())
        return it->second;
    return nullptr;
}

} // namespace sihd::lua