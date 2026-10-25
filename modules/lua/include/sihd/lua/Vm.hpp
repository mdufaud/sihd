#ifndef __SIHD_LUA_VM_HPP__
#define __SIHD_LUA_VM_HPP__

// clang-21 with C++ modules raises an error when <limits.h> (via luaconf.h)
// is included inside extern "C" — suppress that diagnostic.
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wmodule-import-in-extern-c"
#endif
extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#ifdef __clang__
# pragma clang diagnostic pop
#endif

#include <cstdint> // need this for LuaBridge
#include <cstdio>
#include <memory>
#include <string>

#include <luabridge3/LuaBridge/LuaBridge.h>

#include <sihd/lua/LuaGil.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::lua
{

class ILuaThreadStateHandler
{
    public:
        virtual ~ILuaThreadStateHandler() = default;
        virtual lua_State *lua_state() const = 0;
};

class Vm: public ILuaThreadStateHandler
{
    public:
        Vm();
        Vm(lua_State *state);
        virtual ~Vm();

        bool new_state();
        void close_state();
        void set_state(lua_State *state);

        lua_State *new_luathread();
        bool new_thread(Vm & vm);

        luabridge::LuaRef new_table();
        luabridge::LuaRef get_ref(const std::string & name);
        template <typename T>
        void set_ref(const std::string & name, T obj)
        {
            luabridge::setGlobal(_state_ptr, obj, name.c_str());
        }
        bool ref_exists(const std::string & name);
        bool refs_exists(const std::initializer_list<std::string> & lst);

        sihd::util::Result<void> do_file(const std::string & path);
        sihd::util::Result<void> do_string(const std::string & str);

        std::string dump_stack(int max = -1);
        void print_stack(int max = -1, FILE *output = stdout);

        lua_State *lua_state() const override;
        LuaGil *gil() const;

        static Vm *get_vm(lua_State *state);
        static LuaGil *get_gil(lua_State *state);

    protected:

    private:
        bool _state_ownership;
        lua_State *_state_ptr;
        // owned only by the state-creating Vm; borrowed by coroutine/non-owning Vm
        std::unique_ptr<LuaGil> _gil_storage;
        LuaGil *_gil = nullptr;
        int _coroutine_ref = LUA_NOREF;
        lua_State *_coroutine_parent = nullptr;
};

} // namespace sihd::lua

template <>
struct luabridge::Stack<sihd::util::ErrorCode>: luabridge::Enum<sihd::util::ErrorCode,
                                                                sihd::util::ErrorCode::none,
                                                                sihd::util::ErrorCode::unknown,
                                                                sihd::util::ErrorCode::invalid_argument,
                                                                sihd::util::ErrorCode::not_found,
                                                                sihd::util::ErrorCode::permission_denied,
                                                                sihd::util::ErrorCode::timeout,
                                                                sihd::util::ErrorCode::would_block,
                                                                sihd::util::ErrorCode::interrupted,
                                                                sihd::util::ErrorCode::out_of_memory,
                                                                sihd::util::ErrorCode::io_error,
                                                                sihd::util::ErrorCode::not_supported,
                                                                sihd::util::ErrorCode::already_exists,
                                                                sihd::util::ErrorCode::not_initialized,
                                                                sihd::util::ErrorCode::closed,
                                                                sihd::util::ErrorCode::overflow>
{
};

#endif