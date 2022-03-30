#ifndef __SIHD_LUA_LUACOREAPI_HPP__
# define __SIHD_LUA_LUACOREAPI_HPP__

# include <sihd/lua/LuaApi.hpp>
# include <LuaBridge/Vector.h>
# include <LuaBridge/Map.h>
# include <LuaBridge/UnorderedMap.h>

# include <sihd/util/Logger.hpp>

# include <sihd/util/Array.hpp>
# include <sihd/util/Configurable.hpp>

# include <sihd/util/Task.hpp>
# include <sihd/util/IHandler.hpp>

namespace sihd::lua
{

template <typename T>
struct EnumWrapper
{
    static typename std::enable_if<std::is_enum<T>::value, void>::type push(lua_State *L, T value)
    {
        lua_pushnumber(L, static_cast<std::size_t>(value));
    }

    static typename std::enable_if<std::is_enum<T>::value, T>::type get(lua_State *L, int index)
    {
        return static_cast <T>(lua_tointeger(L, index));
    }
};

class LuaUtilApi
{
    public:
        static void load(lua_State *state);

        static sihd::util::Logger logger;

        template <typename T>
        static bool configurable_set_conf(T *obj, luabridge::LuaRef tbl, lua_State *state)
        {
            static_assert(std::is_base_of<sihd::util::Configurable, T>::value);
            if (tbl.isTable() == false)
                luaL_error(state, "set_conf argument must be a table");
            bool ret = true;
            for (const auto & pair: luabridge::pairs(tbl))
            {
                if (pair.first.isString() == false)
                    luaL_error(state, "set_conf keys must be strings");
                std::string key = pair.first.cast<std::string>();
                ret = LuaUtilApi::_configurable_recursive_set(obj, key, pair.second) && ret;
            }
            return ret;
        }

        class LuaTask: public sihd::util::Task
        {
            public:
                LuaTask(luabridge::LuaRef lua_ref, time_t timestamp_to_run = 0, time_t reschedule_every = 0);
                ~LuaTask();

                bool run();

                luabridge::LuaRef fun;
        };

        class LuaRunnable: public sihd::util::IRunnable
        {
            public:
                LuaRunnable(luabridge::LuaRef lua_ref);
                ~LuaRunnable();

                bool run();

                luabridge::LuaRef fun;
        };

        template <typename ...T>
        class LuaHandler: public sihd::util::IHandler<T...>
        {
            public:
                LuaHandler(luabridge::LuaRef lua_ref): fun(lua_ref) {}
                ~LuaHandler() {}

                void handle(T... args)
                {
                    if (this->fun.isFunction())
                        this->fun(args...);
                }

                luabridge::LuaRef fun;
        };

    private:
        LuaUtilApi() {};
        virtual ~LuaUtilApi() {};

        template <int I>
        static constexpr int _get_int()
        {
            return I;
        }

        template <typename T>
        static sihd::util::Array<T> _create_array_from_ref(luabridge::LuaRef ref)
        {
            sihd::util::Array<T> array;
            if (ref.isTable())
            {
                array.reserve(ref.length());
                for (const auto & pair: luabridge::pairs(ref))
                {
                    array.push_back(pair.second.cast<T>());
                }
            }
            return array;
        }

        static bool _configurable_recursive_set(sihd::util::Configurable *obj, const std::string & key, luabridge::LuaRef ref);

};

}

#endif
