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

#include <sihd/util/endian.hpp>

#include <mutex>
#include <queue>
#include <sihd/util/platform.hpp>
#include <sihd/util/version.hpp>

#include <luabridge3/LuaBridge/Map.h>
#include <luabridge3/LuaBridge/UnorderedMap.h>
#include <luabridge3/LuaBridge/Vector.h>

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

}; // namespace luabridge

// enable enums in Lua
template <>
struct luabridge::Stack<sihd::util::Type>: luabridge::Enum<sihd::util::Type,
                                                           sihd::util::Type::TYPE_NONE,
                                                           sihd::util::Type::TYPE_BOOL,
                                                           sihd::util::Type::TYPE_CHAR,
                                                           sihd::util::Type::TYPE_BYTE,
                                                           sihd::util::Type::TYPE_UBYTE,
                                                           sihd::util::Type::TYPE_SHORT,
                                                           sihd::util::Type::TYPE_USHORT,
                                                           sihd::util::Type::TYPE_INT,
                                                           sihd::util::Type::TYPE_UINT,
                                                           sihd::util::Type::TYPE_LONG,
                                                           sihd::util::Type::TYPE_ULONG,
                                                           sihd::util::Type::TYPE_FLOAT,
                                                           sihd::util::Type::TYPE_DOUBLE,
                                                           sihd::util::Type::TYPE_OBJECT>
{
};

template <>
struct luabridge::Stack<sihd::util::ServiceController::State>
    : luabridge::Enum<sihd::util::ServiceController::State,
                      sihd::util::ServiceController::State::None,
                      sihd::util::ServiceController::State::Configuring,
                      sihd::util::ServiceController::State::Configured,
                      sihd::util::ServiceController::State::Initializing,
                      sihd::util::ServiceController::State::Starting,
                      sihd::util::ServiceController::State::Running,
                      sihd::util::ServiceController::State::Stopping,
                      sihd::util::ServiceController::State::Stopped,
                      sihd::util::ServiceController::State::Resetting,
                      sihd::util::ServiceController::State::Error>
{
};

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
                std::string key = std::string(pair.first);
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
                    return R(_fun(args...));
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
                ~LuaHandler() = default;

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
                bool start_worker(std::string_view name);

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
                bool start_worker(std::string_view name);

            private:
                lua_State *_state_ptr;
                LuaRunnable _lua_runnable;
        };

    private:
        LuaUtilApi() = delete;
        ~LuaUtilApi() = delete;

        // Array utils

        template <typename T>
        static sihd::util::Array<T> _array_lua_new(luabridge::LuaRef ref, lua_State *state)
        {
            sihd::util::Array<T> array;
            if constexpr (std::is_same_v<T, char>)
            {
                if (ref.isString())
                {
                    array.from(std::string(ref));
                    return array;
                }
            }
            if (ref.isTable())
            {
                array.reserve(ref.length());
                for (const auto & pair : luabridge::pairs(ref))
                    array.push_back(T(pair.second));
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
                    self->push_back(std::string(ref));
                    return true;
                }
            }
            if (ref.isTable())
            {
                self->reserve(self->size() + ref.length());
                for (const auto & pair : luabridge::pairs(ref))
                    ret = ret && self->push_back(T(pair.second));
            }
            else
                ret = self->push_back(T(ref));
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
                    self->push_front(std::string(ref));
                    return true;
                }
            }
            if (ref.isTable())
            {
                self->reserve(self->size() + ref.length());
                for (const auto & pair : luabridge::pairs(ref))
                    ret = ret && self->push_front(T(pair.second));
            }
            else
                ret = self->push_front(T(ref));
            return ret;
        }

        template <typename T>
        static bool _array_lua_copy_table(sihd::util::Array<T> *self,
                                          luabridge::LuaRef ref,
                                          luabridge::LuaRef from_ref)
        {
            size_t from_idx = 0;
            if (from_ref.isNil() == false)
                from_idx = static_cast<size_t>(from_ref);
            if constexpr (std::is_same_v<T, char>)
            {
                if (ref.isString())
                {
                    self->copy_from(std::string(ref), from_idx);
                    return true;
                }
            }
            if (ref.isTable() == false)
                return false;
            if ((size_t)ref.length() > (from_idx + self->size()))
                return false;
            for (const auto & pair : luabridge::pairs(ref))
            {
                self->set(from_idx, T(pair.second));
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
                    self->from(std::string(ref));
                    return true;
                }
            }
            self->delete_buffer();
            if (ref.isTable())
            {
                self->reserve(self->size() + ref.length());
                for (const auto & pair : luabridge::pairs(ref))
                    ret = ret && self->push_back(T(pair.second));
            }
            else
                ret = self->push_back(T(ref));
            return ret;
        }

        // Configurable's recursive setting
        static bool _configurable_recursive_set(sihd::util::Configurable *obj,
                                                const std::string & key,
                                                luabridge::LuaRef ref);
};

} // namespace sihd::lua

#endif
