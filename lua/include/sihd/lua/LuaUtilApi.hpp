#ifndef __SIHD_LUA_LUACOREAPI_HPP__
# define __SIHD_LUA_LUACOREAPI_HPP__

# include <sol/sol.hpp>
# include <sihd/util/Logger.hpp>

namespace sihd::lua
{

class LuaUtilApi
{
    public:
        static void load(sol::state & state);

        static sihd::util::Logger logger;
    private:
        LuaUtilApi() {};
        virtual ~LuaUtilApi() {};

};

}

#endif 
