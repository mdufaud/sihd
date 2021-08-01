#ifndef __SIHD_LUA_LUACOREAPI_HPP__
# define __SIHD_LUA_LUACOREAPI_HPP__

# include <sol/sol.hpp>

namespace sihd::lua
{

class LuaUtilApi
{
    public:
        LuaUtilApi();
        virtual ~LuaUtilApi();

        static void load(sol::state & state);

    private:
};

}

#endif 
