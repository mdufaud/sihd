#ifndef __SIHD_LUA_VM_HPP__
# define __SIHD_LUA_VM_HPP__

# include <lua.h>
# include <lauxlib.h>
# include <lualib.h>
# include <LuaBridge/LuaBridge.h>

# include <string>
# include <string_view>
# include <mutex>

namespace sihd::lua
{

class ILuaThreadStateHandler
{
    public:
        virtual ~ILuaThreadStateHandler() {}
        virtual lua_State *lua_state() const = 0;
};

class Vm: public ILuaThreadStateHandler
{
    public:
        Vm();
        virtual ~Vm();

        bool new_state();
        lua_State *new_thread();

        luabridge::LuaRef new_table();
        luabridge::LuaRef get_ref(std::string_view name);

        template <typename T>
        void set_ref(std::string_view name, T obj)
        {
            luabridge::setGlobal(_state_ptr, obj, name.data());
        }

        bool do_file(std::string_view path);
        bool do_string(std::string_view str);

        std::string last_string();

        lua_State *lua_state() const { return _state_ptr; }
        std::lock_guard<std::mutex> lock();

    protected:

    private:
        lua_State *_state_ptr;
        std::mutex _mutex;
};

}

#endif