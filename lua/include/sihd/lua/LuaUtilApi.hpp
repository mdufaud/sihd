#ifndef __SIHD_LUA_LUAUTILAPI_HPP__
# define __SIHD_LUA_LUAUTILAPI_HPP__

# include <sihd/lua/Vm.hpp>

# include <sihd/util/Logger.hpp>

# include <sihd/util/Array.hpp>
# include <sihd/util/Configurable.hpp>

# include <sihd/util/Task.hpp>
# include <sihd/util/Scheduler.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/IReader.hpp>
# include <sihd/util/IWriter.hpp>

# include <sihd/util/version.hpp>
# include <sihd/util/platform.hpp>
# include <sihd/util/Endian.hpp>

# include <memory>

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
        // loads api into lua_state
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);
        static void load_tools(Vm & vm);
        static void load_threading(Vm & vm);
        static void load_files(Vm & vm);
        static void load_process(Vm & vm);

        // lua sihd.util.log 's logger
        static sihd::util::Logger logger;

        static std::string dir;

        /**
         * Inheritance APIs
         *  due to luabridge's single inheritance, interfaces probably won't be inherited
         *  here are methods available to add manually into luabridge classes
         */

        template <typename T>
        static bool configurable_set_conf(T *self, luabridge::LuaRef tbl, lua_State *state)
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
                ret = LuaUtilApi::_configurable_recursive_set(self, key, pair.second) && ret;
            }
            return ret;
        }

        template <typename T>
        static luabridge::LuaRef ireader_get_read_data(T *reader, lua_State *state)
        {
            static_assert(std::is_base_of<sihd::util::IReader, T>::value);
            char *data = nullptr;
            size_t size = 0;
            if (reader->get_read_data(&data, &size))
            {
                sihd::util::ArrStr array;
                array.assign(data, size);
                return luabridge::LuaRef(state, array);
            }
            return luabridge::LuaRef(state);
        }

        /**
         * sihd wrapped into lua
         */

        class LuaScheduler: public sihd::util::Scheduler, public ILuaThreadStateHandler
        {
            public:
                LuaScheduler(const std::string & name, sihd::util::Node *parent = nullptr);
                ~LuaScheduler();

                void set_vm(Vm *vm_ptr);
                bool run();

                lua_State *lua_state() const;

            private:
                Vm *_vm_ptr;
                lua_State *_state_ptr;
        };

        class LuaTask: public sihd::util::Task
        {
            public:
                LuaTask(ILuaThreadStateHandler *handler, luabridge::LuaRef lua_ref,
                            time_t timestamp_to_run = 0, time_t reschedule_every = 0);
                ~LuaTask();

                bool run();

            private:
                ILuaThreadStateHandler *_lua_thread_handler_ptr;
                luabridge::LuaRef _fun;
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

        // used because no variables or const can be set into namespaces
        template <int I>
        static constexpr int _get_int()
        {
            return I;
        }

        static constexpr const char *_get_version_str()
        {
            return SIHD_VERSION_STRING;
        }

        static constexpr const char *_get_platform_str()
        {
            return __SIHD_PLATFORM__;
        }

        template <sihd::util::Endian::Endianness E>
        static constexpr bool _is_endian()
        {
            return sihd::util::Endian::get_endian() == E;
        }

        // Array utils

        template <typename T>
        static sihd::util::Array<T> _array_lua_new(luabridge::LuaRef ref, lua_State *state)
        {
            sihd::util::Array<T> array;
            if (ref.isTable())
            {
                array.reserve(ref.length());
                for (const auto & pair: luabridge::pairs(ref))
                    array.push_back(pair.second.cast<T>());
            }
            else
                luaL_error(state, "Array new argument must be a table");
            return array;
        }

        template <typename T>
        static bool _array_lua_push_back(sihd::util::Array<T> *self, luabridge::LuaRef ref)
        {
            bool ret = true;
            if (ref.isTable())
            {
                self->reserve(self->size() + ref.length());
                for (const auto & pair: luabridge::pairs(ref))
                    ret = ret && self->push_back(pair.second.cast<T>());
            }
            else
                ret = self->push_back(ref.cast<T>());
            return ret;
        }

        template <typename T>
        static bool _array_lua_copy_table(sihd::util::Array<T> *self, luabridge::LuaRef tbl, luabridge::LuaRef from)
        {
            if (tbl.isTable() == false)
                return false;
            size_t from_idx = 0;
            if (from.isNil() == false)
                from_idx = from.cast<size_t>();
            if ((size_t)tbl.length() > (from_idx + self->size()))
                return false;
            for (const auto & pair: luabridge::pairs(tbl))
            {
                self->set(from_idx, pair.second.cast<T>());
                ++from_idx;
            }
            return true;
        }

        // Configurable's recursive setting
        static bool _configurable_recursive_set(sihd::util::Configurable *obj, const std::string & key, luabridge::LuaRef ref);

};

}

#endif
