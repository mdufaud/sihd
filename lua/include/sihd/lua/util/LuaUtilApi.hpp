#ifndef __SIHD_LUA_LUAUTILAPI_HPP__
#define __SIHD_LUA_LUAUTILAPI_HPP__

#include <sihd/lua/Vm.hpp>

#include <sihd/util/Logger.hpp>
#include <sihd/util/ServiceController.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include <sihd/util/Array.hpp>
#include <sihd/util/Configurable.hpp>

#include <sihd/util/Scheduler.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>

#include <sihd/util/IHandler.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/IWriter.hpp>

#include <sihd/util/Endian.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/version.hpp>

#include <LuaBridge/Map.h>
#include <LuaBridge/UnorderedMap.h>
#include <LuaBridge/Vector.h>
#include <LuaBridge/detail/Stack.h>

#include <memory>

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
            return static_cast<T>(lua_tointeger(L, index));
        }
};

} // namespace sihd::lua

namespace luabridge
{
// LuaBridge smart pointer management for Named/Node pattern
template <class C>
struct ContainerTraits<sihd::util::SmartNodePtr<C>>
{
        using Type = C;

        static sihd::util::SmartNodePtr<C> construct(C *obj) { return obj; }

        static C *get(sihd::util::SmartNodePtr<C> & obj) { return obj.get(); }
};

// std::string_view as a pushable lua class
template <>
struct Stack<std::string_view>
{
        static void push(lua_State *L, const std::string_view & str) { lua_pushlstring(L, str.data(), str.size()); }

        static std::string_view get(lua_State *L, int index)
        {
            size_t len;
            if (lua_type(L, index) == LUA_TSTRING)
            {
                const char *str = lua_tolstring(L, index, &len);
                return std::string_view(str, len);
            }
            // Lua reference manual:
            // If the value is a number, then lua_tolstring also changes the actual value in the stack to a string.
            //(This change confuses lua_next when lua_tolstring is applied to keys during a table traversal.)
            lua_pushvalue(L, index);
            const char *str = lua_tolstring(L, -1, &len);
            std::string_view string(str, len);
            lua_pop(L, 1); // Pop the temporary string
            return string;
        }

        static bool isInstance(lua_State *L, int index) { return lua_type(L, index) == LUA_TSTRING; }
};

// int8_t was missing ??
template <>
struct Stack<int8_t>
{
        static void push(lua_State *L, int8_t value) { lua_pushnumber(L, static_cast<int8_t>(value)); }

        static int8_t get(lua_State *L, int index) { return static_cast<int8_t>(lua_tointeger(L, index)); }
};

// enable enums in Lua

template <>
struct Stack<sihd::util::Type>: sihd::lua::EnumWrapper<sihd::util::Type>
{
};

template <>
struct Stack<sihd::util::ServiceController::State>: sihd::lua::EnumWrapper<sihd::util::ServiceController::State>
{
};

}; // namespace luabridge

namespace sihd::lua
{

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
            for (const auto & pair : luabridge::pairs(tbl))
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
            sihd::util::ArrCharView view;
            if (reader->get_read_data(view))
            {
                sihd::util::ArrChar array = view;
                return luabridge::LuaRef(state, array);
            }
            return luabridge::LuaRef(state);
        }

        /**
         * sihd wrapped into lua
         */

        class LuaThreadRunner
        {
            public:
                LuaThreadRunner(luabridge::LuaRef lua_ref);
                virtual ~LuaThreadRunner();

                void new_lua_state(lua_State *state);

                template <typename... T>
                void call_lua_method_noret(T... args)
                {
                    _fun(args...);
                }

                template <typename R, typename... T>
                R call_lua_method(T... args)
                {
                    return _fun(args...);
                }

            private:
                luabridge::LuaRef _original_fun;
                luabridge::LuaRef _fun;
        };

        class LuaTask: public sihd::util::Task,
                       public LuaThreadRunner
        {
            public:
                LuaTask(luabridge::LuaRef lua_ref, const sihd::util::TaskOptions & options);
                ~LuaTask();

                virtual bool run() override;
        };

        class LuaRunnable: public sihd::util::IRunnable,
                           public LuaThreadRunner
        {
            public:
                LuaRunnable(luabridge::LuaRef lua_ref);
                ~LuaRunnable();

                virtual bool run() override;
        };

        template <typename... T>
        class LuaHandler: public sihd::util::IHandler<T...>,
                          public LuaThreadRunner
        {
            public:
                LuaHandler(luabridge::LuaRef lua_ref): LuaThreadRunner(lua_ref) {}
                ~LuaHandler() {}

                void handle(T... args)
                {
                    // ISO C++03 14.2/4
                    this->template call_lua_method<void, T...>(args...);
                }
        };

        class LuaScheduler: public sihd::util::Scheduler
        {
            public:
                LuaScheduler(const std::string & name, sihd::util::Node *parent = nullptr);
                ~LuaScheduler();

                void set_state(lua_State *state);
                void add_lua_task(LuaTask *task_ptr);

                virtual bool start() override;
                virtual bool stop() override;

            protected:

            private:
                lua_State *_state_ptr;
                Vm _vm_thread;
        };

        class LuaWorker: public sihd::util::Worker
        {
            public:
                LuaWorker(luabridge::LuaRef ref);
                ~LuaWorker();

                void set_state(lua_State *state);
                bool start_worker(const std::string_view name) override;

            private:
                lua_State *_state_ptr;
                LuaRunnable _lua_runnable;
        };

        class LuaStepWorker: public sihd::util::StepWorker
        {
            public:
                LuaStepWorker(luabridge::LuaRef ref);
                ~LuaStepWorker();

                void set_state(lua_State *state);
                bool start_worker(const std::string_view name) override;

            private:
                lua_State *_state_ptr;
                LuaRunnable _lua_runnable;
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

        // used because no variables or const can be set into namespaces
        template <bool B>
        static constexpr bool _get_bool()
        {
            return B;
        }

        static constexpr const char *_get_version_str() { return SIHD_VERSION_STRING; }

        static constexpr const char *_get_platform_str() { return __SIHD_PLATFORM__; }

        template <sihd::util::Endian::Endianness E>
        static constexpr bool _is_endian()
        {
            return sihd::util::Endian::endian() == E;
        }

        // Array utils

        template <typename T>
        static sihd::util::Array<T> _array_lua_new(luabridge::LuaRef ref, lua_State *state)
        {
            sihd::util::Array<T> array;
            if constexpr (std::is_same_v<T, char>)
            {
                if (ref.isString())
                {
                    array.from(ref.cast<std::string>());
                    return array;
                }
            }
            if (ref.isTable())
            {
                array.reserve(ref.length());
                for (const auto & pair : luabridge::pairs(ref))
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
            if constexpr (std::is_same_v<T, char>)
            {
                if (ref.isString())
                {
                    self->push_back(ref.cast<std::string>());
                    return true;
                }
            }
            if (ref.isTable())
            {
                self->reserve(self->size() + ref.length());
                for (const auto & pair : luabridge::pairs(ref))
                    ret = ret && self->push_back(pair.second.cast<T>());
            }
            else
                ret = self->push_back(ref.cast<T>());
            return ret;
        }

        template <typename T>
        static bool _array_lua_push_front(sihd::util::Array<T> *self, luabridge::LuaRef ref)
        {
            bool ret = true;
            if constexpr (std::is_same_v<T, char>)
            {
                if (ref.isString())
                {
                    self->push_front(ref.cast<std::string>());
                    return true;
                }
            }
            if (ref.isTable())
            {
                self->reserve(self->size() + ref.length());
                for (const auto & pair : luabridge::pairs(ref))
                    ret = ret && self->push_front(pair.second.cast<T>());
            }
            else
                ret = self->push_front(ref.cast<T>());
            return ret;
        }

        template <typename T>
        static bool _array_lua_copy_table(sihd::util::Array<T> *self, luabridge::LuaRef ref, luabridge::LuaRef from_ref)
        {
            size_t from_idx = 0;
            if (from_ref.isNil() == false)
                from_idx = from_ref.cast<size_t>();
            if constexpr (std::is_same_v<T, char>)
            {
                if (ref.isString())
                {
                    self->copy_from(ref.cast<std::string>(), from_idx);
                    return true;
                }
            }
            if (ref.isTable() == false)
                return false;
            if ((size_t)ref.length() > (from_idx + self->size()))
                return false;
            for (const auto & pair : luabridge::pairs(ref))
            {
                self->set(from_idx, pair.second.cast<T>());
                ++from_idx;
            }
            return true;
        }

        template <typename T>
        static bool _array_lua_from(sihd::util::Array<T> *self, luabridge::LuaRef ref)
        {
            bool ret = true;
            if constexpr (std::is_same_v<T, char>)
            {
                if (ref.isString())
                {
                    self->from(ref.cast<std::string>());
                    return true;
                }
            }
            self->delete_buffer();
            if (ref.isTable())
            {
                self->reserve(self->size() + ref.length());
                for (const auto & pair : luabridge::pairs(ref))
                    ret = ret && self->push_back(pair.second.cast<T>());
            }
            else
                ret = self->push_back(ref.cast<T>());
            return ret;
        }

        // Configurable's recursive setting
        static bool
            _configurable_recursive_set(sihd::util::Configurable *obj, const std::string & key, luabridge::LuaRef ref);
};

} // namespace sihd::lua

#endif
