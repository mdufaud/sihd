#include <fmt/core.h>

#include <sihd/lua/Vm.hpp>
#include <sihd/lua/checked.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>

using namespace sihd::util;
using enum ErrorCode;

namespace sihd::lua
{

SIHD_NEW_LOGGER("sihd::lua");

namespace
{

std::mutex g_mutex;
std::map<lua_State *, Vm *> g_map_vm;
// state -> GIL lookup, kept in C++ so the lock is found without touching the lua_State
std::map<lua_State *, LuaGil *> g_map_gil;

void register_vm(lua_State *state, Vm *vm)
{
    std::lock_guard lock(g_mutex);
    g_map_vm[state] = vm;
}

void unregister_vm(lua_State *state)
{
    std::lock_guard lock(g_mutex);
    g_map_vm.erase(state);
}

void register_gil(lua_State *state, LuaGil *gil)
{
    std::lock_guard lock(g_mutex);
    g_map_gil[state] = gil;
}

void unregister_gil(lua_State *state)
{
    std::lock_guard lock(g_mutex);
    g_map_gil.erase(state);
}

void unregister_gil_universe(LuaGil *gil)
{
    std::lock_guard lock(g_mutex);
    std::erase_if(g_map_gil, [gil](const auto & pair) { return pair.second == gil; });
}

// luabridge throws on an unregistered class: its Stack must not be asked before the class exists
bool error_class_registered(lua_State *state)
{
    lua_rawgetp(state, LUA_REGISTRYINDEX, luabridge::detail::getClassRegistryKey<Error>());
    const bool registered = lua_istable(state, -1);
    lua_pop(state, 1);
    return registered;
}

void register_error_class(lua_State *state)
{
    luabridge::getGlobalNamespace(state)
        .beginNamespace("sihd")
        .beginClass<Error>("Error")
        .addProperty(
            "code",
            +[](const Error *self) { return static_cast<int>(self->code); })
        .addProperty(
            "message",
            +[](const Error *self) { return self->message; })
        .addFunction("retryable", &Error::retryable)
        .addFunction(
            "__tostring",
            +[](const Error *self) { return self->message; })
        .endClass()
        .beginNamespace("ErrorCode")
        .addVariable("none", ErrorCode::none)
        .addVariable("unknown", ErrorCode::unknown)
        .addVariable("invalid_argument", ErrorCode::invalid_argument)
        .addVariable("not_found", ErrorCode::not_found)
        .addVariable("permission_denied", ErrorCode::permission_denied)
        .addVariable("timeout", ErrorCode::timeout)
        .addVariable("would_block", ErrorCode::would_block)
        .addVariable("interrupted", ErrorCode::interrupted)
        .addVariable("out_of_memory", ErrorCode::out_of_memory)
        .addVariable("io_error", ErrorCode::io_error)
        .addVariable("not_supported", ErrorCode::not_supported)
        .addVariable("already_exists", ErrorCode::already_exists)
        .addVariable("not_initialized", ErrorCode::not_initialized)
        .addVariable("closed", ErrorCode::closed)
        .addVariable("overflow", ErrorCode::overflow)
        .endNamespace()
        .endNamespace();
}

Error pop_lua_error(ErrorCode code, lua_State *state)
{
    if (error_class_registered(state))
    {
        if (auto raised = luabridge::Stack<Error>::get(state, -1))
        {
            Error error = *raised;
            lua_pop(state, 1);
            return error;
        }
    }
    const char *msg = lua_tostring(state, -1);
    lua_pop(state, 1);
    return Error(code, msg == nullptr ? "unknown lua error" : msg);
}

Result<void> do_chunk(lua_State *state, int load_status, ErrorCode load_code)
{
    if (load_status != 0)
        return std::unexpected(pop_lua_error(load_code, state));
    if (lua_pcall(state, 0, LUA_MULTRET, 0) != 0)
        return std::unexpected(pop_lua_error(unknown, state));
    return {};
}

} // namespace

int push_error(lua_State *state, const Error & error)
{
    if (!error_class_registered(state))
        register_error_class(state);
    if (!luabridge::Stack<Error>::push(state, error))
        lua_pushlstring(state, error.message.data(), error.message.size());
    return -1;
}

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
    {
        _gil = get_gil(state);
        register_vm(state, this);
    }
}

void Vm::close_state()
{
    if (_state_ptr != nullptr)
    {
        unregister_vm(_state_ptr);
        if (_coroutine_ref != LUA_NOREF && _coroutine_parent != nullptr)
        {
            {
                LuaGilGuard guard(_coroutine_parent);
                luaL_unref(_coroutine_parent, LUA_REGISTRYINDEX, _coroutine_ref);
            }
            unregister_gil(_state_ptr);
        }
        if (_state_ownership)
        {
            unregister_gil_universe(_gil);
            lua_close(_state_ptr);
        }
    }
    _coroutine_ref = LUA_NOREF;
    _coroutine_parent = nullptr;
    _gil = nullptr;
    _gil_storage.reset();
    _state_ptr = nullptr;
}

bool Vm::new_state()
{
    this->close_state();
    _state_ptr = luaL_newstate();
    if (_state_ptr != nullptr)
    {
        _gil_storage = std::make_unique<LuaGil>();
        _gil = _gil_storage.get();
        register_gil(_state_ptr, _gil);
        register_vm(_state_ptr, this);
        _state_ownership = true;
        luaL_openlibs(_state_ptr);
    }
    return _state_ptr != nullptr;
}

lua_State *Vm::new_luathread()
{
    if (_state_ptr == nullptr)
        return nullptr;
    LuaGilGuard guard(_state_ptr);
    lua_State *thread_state = lua_newthread(_state_ptr);
    if (thread_state != nullptr && _gil != nullptr)
        register_gil(thread_state, _gil);
    return thread_state;
}

bool Vm::new_thread(Vm & vm)
{
    if (_state_ptr == nullptr)
        return false;
    LuaGilGuard guard(_state_ptr);
    lua_State *thread_state = lua_newthread(_state_ptr);
    if (thread_state == nullptr)
        return false;
    // anchor the coroutine in the registry, off the parent stack
    const int ref = luaL_ref(_state_ptr, LUA_REGISTRYINDEX);
    if (_gil != nullptr)
        register_gil(thread_state, _gil);
    vm.set_state(thread_state);
    vm._coroutine_ref = ref;
    vm._coroutine_parent = _state_ptr;
    return true;
}

luabridge::LuaRef Vm::new_table()
{
    return luabridge::newTable(_state_ptr);
}

luabridge::LuaRef Vm::get_ref(const std::string & name)
{
    return luabridge::getGlobal(_state_ptr, name.c_str());
}

bool Vm::ref_exists(const std::string & name)
{
    lua_getglobal(_state_ptr, name.c_str());
    const bool exists = lua_isnil(_state_ptr, -1) == 0;
    lua_pop(_state_ptr, 1);
    return exists;
}

bool Vm::refs_exists(const std::initializer_list<std::string> & lst)
{
    if (lst.size() == 0)
        return false;
    bool first = true;
    for (const std::string & str : lst)
    {
        if (first)
        {
            lua_getglobal(_state_ptr, str.c_str());
            first = false;
        }
        else
        {
            if (lua_istable(_state_ptr, -1) == 0)
            {
                lua_remove(_state_ptr, -1);
                return false;
            }
            lua_getfield(_state_ptr, -1, str.c_str());
            lua_remove(_state_ptr, -2);
        }
    }
    bool ret = lua_isnil(_state_ptr, -1) == 0;
    lua_remove(_state_ptr, -1);
    return ret;
}

Result<void> Vm::do_file(const std::string & path)
{
    LuaGilGuard guard(_state_ptr);
    const int load_status = luaL_loadfile(_state_ptr, path.c_str());
    ErrorCode load_code = out_of_memory;
    if (load_status == LUA_ERRFILE)
        load_code = not_found;
    else if (load_status == LUA_ERRSYNTAX)
        load_code = invalid_argument;
    return do_chunk(_state_ptr, load_status, load_code);
}

Result<void> Vm::do_string(const std::string & str)
{
    LuaGilGuard guard(_state_ptr);
    const int load_status = luaL_loadstring(_state_ptr, str.c_str());
    const ErrorCode load_code = load_status == LUA_ERRSYNTAX ? invalid_argument : out_of_memory;
    return do_chunk(_state_ptr, load_status, load_code);
}

lua_State *Vm::lua_state() const
{
    return _state_ptr;
}

LuaGil *Vm::gil() const
{
    return _gil;
}

void Vm::print_stack(int max, FILE *output)
{
    fmt::print(output, "{}", this->dump_stack(max));
}

std::string Vm::dump_stack(int max)
{
    if (_state_ptr == nullptr)
        return "";
    std::string dump;
    int top = lua_gettop(_state_ptr);
    if (max >= 0)
        top = std::min(top, max);
    dump = fmt::format("lua stack {} ({}):\n", static_cast<void *>(_state_ptr), top);
    int i = 1;
    while (i <= top)
    {
        dump += fmt::format("stack[{}]:\t", i);
        switch (lua_type(_state_ptr, i))
        {
            case LUA_TNUMBER:
                dump += fmt::format("{}", lua_tonumber(_state_ptr, i));
                break;
            case LUA_TSTRING:
                dump += fmt::format("{}", lua_tostring(_state_ptr, i));
                break;
            case LUA_TBOOLEAN:
                dump += fmt::format("{}", (lua_toboolean(_state_ptr, i) ? "true" : "false"));
                break;
            case LUA_TNIL:
                dump += "nil";
                break;
            default:
                dump += fmt::format("{}", lua_topointer(_state_ptr, i));
                break;
        }
        dump += fmt::format(" ({})\n", luaL_typename(_state_ptr, i));
        ++i;
    }
    return dump;
}

Vm *Vm::get_vm(lua_State *state)
{
    std::lock_guard lock(g_mutex);
    auto it = g_map_vm.find(state);
    if (it != g_map_vm.end())
        return it->second;
    return nullptr;
}

LuaGil *Vm::get_gil(lua_State *state)
{
    std::lock_guard lock(g_mutex);
    auto it = g_map_gil.find(state);
    if (it != g_map_gil.end())
        return it->second;
    return nullptr;
}

} // namespace sihd::lua
