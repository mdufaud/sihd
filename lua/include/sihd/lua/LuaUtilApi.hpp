#ifndef __SIHD_LUA_LUACOREAPI_HPP__
# define __SIHD_LUA_LUACOREAPI_HPP__

# include <sol/sol.hpp>
# include <sihd/util/Node.hpp>

namespace sihd::lua
{

class LuaUtilApi
{
    public:
        LuaUtilApi();
        virtual ~LuaUtilApi();

        static void load(sol::state & state);
        static void unload();

        static sihd::util::Node *root;

    private:
};

}

#endif 
