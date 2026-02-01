#ifndef __SIHD_LUA_VM_HPP__
#define __SIHD_LUA_VM_HPP__

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cstdint> // need this for LuaBridge
#include <luabridge3/LuaBridge/LuaBridge.h>

#include <string>
#include <string_view>

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
        // create a lua_State and loads library
        Vm();
        // does not take ownership
        Vm(lua_State *state);
        virtual ~Vm();

        // close any old lua_State and create a lua_State and loads library
        bool new_state();
        // close lua_State if ownership
        void close_state();
        // close any old lua_State and set new unowned state
        void set_state(lua_State *state);

        lua_State *new_luathread();
        bool new_thread(Vm & vm);

        luabridge::LuaRef new_table();
        luabridge::LuaRef get_ref(std::string_view name);
        template <typename T>
        void set_ref(std::string_view name, T obj)
        {
            luabridge::setGlobal(_state_ptr, obj, name.data());
        }
        bool ref_exists(std::string_view name);
        bool refs_exists(const std::initializer_list<std::string_view> & lst);

        bool do_file(std::string_view path);
        bool do_string(std::string_view str);
        std::string last_string();

        std::string dump_stack(int max = -1);
        void print_stack(int max = -1, FILE *output = stdout);

        lua_State *lua_state() const { return _state_ptr; }

        static Vm *get_vm(lua_State *state);

    protected:

    private:
        bool _state_ownership;
        lua_State *_state_ptr;
};

} // namespace sihd::lua

#endif