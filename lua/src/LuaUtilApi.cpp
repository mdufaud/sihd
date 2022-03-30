#include <sihd/lua/LuaUtilApi.hpp>

#include <unistd.h>

#include <LuaBridge/detail/Stack.h>

#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include <sihd/util/Types.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Thread.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/AtExit.hpp>

#include <sihd/util/AService.hpp>
#include <sihd/util/ServiceController.hpp>

#include <sihd/util/Scheduler.hpp>

#include <sihd/util/Process.hpp>

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType) \
    .deriveClass<ArrType, IArray>(#ArrType)\
        .addConstructor<void (*)()>()\
        .addStaticFunction("from", &LuaUtilApi::_create_array_from_ref<PrimitiveType>)\
        .addFunction("clone", &ArrType::clone)\
        .addFunction("push_back", static_cast<bool (ArrType::*)(const PrimitiveType &)>(&ArrType::push_back))\
        .addFunction("pop", &ArrType::pop)\
        .addFunction("front", &ArrType::front)\
        .addFunction("back", &ArrType::back)\
        .addFunction("at", &ArrType::at)\
        .addFunction("set", &ArrType::set)\
        .addFunction("to_string", &ArrType::to_string)\
        .addFunction("__len", &ArrType::size)\
    .endClass()

namespace luabridge
{
    template <class C>
    struct ContainerTraits <sihd::util::SmartNodePtr<C>>
    {
        using Type = C;

        static sihd::util::SmartNodePtr<C> construct(C* obj)
        {
            return obj;
        }

        static C *get(sihd::util::SmartNodePtr<C> & obj)
        {
            return obj.get();
        }
    };

    // template <typename T>
    // struct Stack<sihd::util::Array<T>>
    // {
    //     static void push(lua_State* L, const sihd::util::Array<T> & array)
    //     {
    //         lua_createtable(L, static_cast<int>(array.size()), 0);
    //         for (std::size_t i = 0; i < array.size(); ++i)
    //         {
    //             lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
    //             Stack<T>::push(L, array[i]);
    //             lua_settable(L, -3);
    //         }
    //     }

    //     static sihd::util::Array<T> get(lua_State* L, int index)
    //     {
    //         if (!lua_istable(L, index))
    //         {
    //             luaL_error(L, "#%d argument must be a table", index);
    //         }
    //         sihd::util::Array<T> array;
    //         array.reserve(static_cast<std::size_t>(get_length(L, index)));

    //         const int absindex = lua_absindex(L, index);
    //         lua_pushnil(L);
    //         while (lua_next(L, absindex) != 0)
    //         {
    //             array.push_back(Stack<T>::get(L, -1));
    //             lua_pop(L, 1);
    //         }
    //         return array;
    //     }

    //     static bool isInstance(lua_State* L, int index)
    //     {
    //         return lua_istable(L, index);
    //     }
    // };

    template <>
    struct Stack<sihd::util::Type> : sihd::lua::EnumWrapper<sihd::util::Type>
    {
    };

    template <>
    struct Stack<sihd::util::ServiceController::State> : sihd::lua::EnumWrapper<sihd::util::ServiceController::State>
    {
    };

};

namespace sihd::lua
{

using namespace sihd::util;

Logger LuaUtilApi::logger("sihd::lua");

SIHD_LOGGER;

bool    LuaUtilApi::_configurable_recursive_set(Configurable *obj, const std::string & key, luabridge::LuaRef ref)
{
    switch (ref.type())
    {
        case LUA_TBOOLEAN:
        {
            return obj->set_conf<bool>(key, ref.cast<bool>());
        }
        case LUA_TTABLE:
        {
            bool ret = true;
            for (const auto & key_pair: luabridge::pairs(ref))
                ret = LuaUtilApi::_configurable_recursive_set(obj, key, key_pair.second) && ret;
            return ret;
        }
        case LUA_TNUMBER:
        {
            try
            {
                return obj->set_conf_float(key, ref.cast<double>());
            }
            catch (const std::invalid_argument & e)
            {
                return obj->set_conf_int(key, ref.cast<int64_t>());
            }
            return false;
        }
        case LUA_TSTRING:
        {
            return obj->set_conf_str(key, ref.cast<std::string>());
        }
        default:
        {
            logger.error(Str::format("Configuration '%s' type error", key.c_str()).c_str());
        }
    }
    return false;
}

void    LuaUtilApi::load(lua_State *state)
{
    luabridge::getGlobalNamespace(state)
        .beginNamespace("sihd")
            .beginNamespace("util")
                /**
                 * Tools
                 */
                .addVariable("clock", &Clock::default_clock)
                .beginClass<IClock>("IClock")
                    .addFunction("now", &IClock::now)
                    .addFunction("is_steady", &IClock::is_steady)
                .endClass()
                .deriveClass<SystemClock, IClock>("SystemClock")
                    .addConstructor<void (*)()>()
                .endClass()
                    .deriveClass<SteadyClock, IClock>("SteadyClock")
                    .addConstructor<void (*)()>()
                .endClass()
                .beginNamespace("log")
                    .addFunction("debug", +[] (const std::string & log) { logger.log(debug, log); })
                    .addFunction("info", +[] (const std::string & log) { logger.log(info, log); })
                    .addFunction("warning", +[] (const std::string & log) { logger.log(warning, log); })
                    .addFunction("error", +[] (const std::string & log) { logger.log(error, log); })
                    .addFunction("critical", +[] (const std::string & log) { logger.log(critical, log); })
                .endNamespace()
                .beginNamespace("time")
                    // sleep
                    .addFunction("nsleep", &time::nsleep)
                    .addFunction("usleep", &time::usleep)
                    .addFunction("msleep", &time::msleep)
                    .addFunction("sleep", &time::sleep)
                    // convert to nano
                    .addFunction("to_us", &time::to_micro)
                    .addFunction("to_ms", &time::to_milli)
                    .addFunction("to_sec", &time::to_sec)
                    .addFunction("to_min", &time::to_min)
                    .addFunction("to_hours", &time::to_hours)
                    .addFunction("to_days", &time::to_days)
                    .addFunction("to_double", &time::to_double)
                    .addFunction("to_hz", &time::to_freq)
                    // convert from nano
                    .addFunction("us", &time::micro)
                    .addFunction("ms", &time::milli)
                    .addFunction("sec", &time::sec)
                    .addFunction("min", &time::min)
                    .addFunction("hours", &time::hours)
                    .addFunction("days", &time::days)
                    .addFunction("from_double", &time::from_double)
                    .addFunction("hz", &time::freq)
                .endNamespace()
                .beginNamespace("types")
                    .addFunction("type_size", &Types::type_size)
                    .addFunction("type_to_string", &Types::type_to_string)
                    .addFunction("string_to_type", &Types::string_to_type)
                .endNamespace()
                .beginNamespace("thread")
                    .addFunction("id", &Thread::id)
                    .addFunction("id_str", static_cast<std::string (*)()>(&Thread::id_str))
                    .addFunction("set_name", &Thread::set_name)
                    .addFunction("get_name", &Thread::get_name)
                .endNamespace()
                .beginNamespace("os")
                    .addProperty("stdin", &LuaUtilApi::_get_int<STDIN_FILENO>)
                    .addProperty("stdout", &LuaUtilApi::_get_int<STDOUT_FILENO>)
                    .addProperty("stderr", &LuaUtilApi::_get_int<STDERR_FILENO>)
                    .addProperty("backtrace_size",
                        +[] () { return OS::backtrace_size; },
                        +[] (int val) { OS::backtrace_size = val; })
                    .addFunction("backtrace", &OS::backtrace)
                    .addFunction("pid", &OS::get_pid)
                    .addFunction("kill", &OS::kill)
                    .addFunction("get_signal_name", &OS::get_signal_name)
                    .addFunction("get_max_fds", &OS::get_max_fds)
                    .addFunction("is_root", &OS::is_root)
                    .addFunction("get_home", &OS::get_home)
                    .addFunction("get_executable_path", &OS::get_executable_path)
                    .addFunction("get_cwd", &OS::get_cwd)
                    .addFunction("is_run_by_debugger", &OS::is_run_by_debugger)
                    .addFunction("is_run_by_valgrind", &OS::is_run_by_valgrind)
                    .addFunction("is_run_with_asan", &OS::is_run_with_asan)
                    .addFunction("get_peak_rss", &OS::get_peak_rss)
                    .addFunction("get_current_rss", &OS::get_current_rss)
                .endNamespace()
                /**
                 * Nodes
                 */
                .beginClass<Node::ChildEntry>("ChildEntry")
                    .addProperty("name", &Node::ChildEntry::name)
                    .addProperty("obj", &Node::ChildEntry::obj)
                    .addProperty("ownership", &Node::ChildEntry::ownership)
                .endClass()
                .beginClass<Named>("Named")
                    .addConstructor<void (*)(const std::string &, Node *), SmartNodePtr<Named>>()
                    .addProperty("c_ptr", +[] (const Named *self) -> int64_t { return (int64_t)self; })
                    .addFunction("get_parent", &Named::get_parent)
                    .addFunction("get_name", &Named::get_name)
                    .addFunction("get_full_name", &Named::get_full_name)
                    .addFunction("get_class_name", &Named::get_class_name)
                    .addFunction("get_description", &Named::get_description)
                    .addFunction("get_full_name", &Named::get_full_name)
                    .addFunction("get_root", &Named::get_root)
                    .addFunction("find", static_cast<Named * (Named::*)(const std::string &)>(&Named::find))
                    .addFunction("find_from", static_cast<Named * (Named::*)(Named *, const std::string &)>(&Named::find))
                    .addFunction("__eq", +[] (const Named *self, const Named *other) -> bool { return self == other; })
                .endClass()
                .deriveClass<Node, Named>("Node")
                    .addConstructor<void (*)(const std::string &, Node *), SmartNodePtr<Node>>()
                    .addFunction("get_child", static_cast<Named *(Node::*)(const std::string &) const>(&Node::get_child))
                    .addFunction("add_child", +[] (Node *self, Named *child) { return self->add_child(child); })
                    .addFunction("add_child_name", +[] (Node *self, const std::string & name, Named *child) { return self->add_child(name, child); })
                    .addFunction("delete_child", static_cast<bool (Node::*)(const Named *)>(&Node::delete_child))
                    .addFunction("delete_child_name", static_cast<bool (Node::*)(const std::string &)>(&Node::delete_child))
                    .addFunction("is_link", &Node::is_link)
                    .addFunction("add_link", &Node::add_link)
                    .addFunction("remove_link", &Node::remove_link)
                    .addFunction("get_link", &Node::get_link)
                    .addFunction("resolve_links", &Node::resolve_links)
                    .addFunction("get_tree_str", static_cast<std::string (Node::*)() const>(&Node::get_tree_str))
                    .addFunction("get_tree_desc_str", &Node::get_tree_desc_str)
                    .addFunction("get_children", &Node::get_children)
                    .addFunction("get_children_keys", &Node::get_children_keys)
                    .addFunction("__eq", +[] (const Node *self, const Named *other) -> bool { return self == other; })
                .endClass()
                /**
                 * Array
                 */
                .beginClass<IArray>("IArray")
                    .addFunction("size", &IArray::size)
                    .addFunction("capacity", &IArray::capacity)
                    .addFunction("data_size", &IArray::data_size)
                    .addFunction("byte_size", &IArray::byte_size)
                    .addFunction("resize", &IArray::resize)
                    .addFunction("reserve", &IArray::reserve)
                    .addFunction("byte_resize", &IArray::byte_resize)
                    .addFunction("byte_reserve", &IArray::byte_reserve)
                    .addFunction("data_type", &IArray::data_type)
                    .addFunction("data_type_to_string", &IArray::data_type_to_string)
                    .addFunction("hexdump", &IArray::hexdump)
                    .addFunction("to_string", &IArray::to_string)
                    .addFunction("clear", &IArray::clear)
                .endClass()
                DECLARE_ARRAY_USERTYPE(ArrBool, bool)
                DECLARE_ARRAY_USERTYPE(ArrChar, char)
                DECLARE_ARRAY_USERTYPE(ArrByte, int8_t)
                DECLARE_ARRAY_USERTYPE(ArrUByte, uint8_t)
                DECLARE_ARRAY_USERTYPE(ArrShort, int16_t)
                DECLARE_ARRAY_USERTYPE(ArrUShort, uint16_t)
                DECLARE_ARRAY_USERTYPE(ArrInt, int32_t)
                DECLARE_ARRAY_USERTYPE(ArrUInt, uint32_t)
                DECLARE_ARRAY_USERTYPE(ArrLong, int64_t)
                DECLARE_ARRAY_USERTYPE(ArrULong, uint64_t)
                DECLARE_ARRAY_USERTYPE(ArrFloat, float)
                DECLARE_ARRAY_USERTYPE(ArrDouble, double)
                // a bit of a specialization for ArrStr
                .deriveClass<ArrStr, IArray>("ArrStr")
                    .addConstructor<void (*)()>()
                    .addStaticFunction("from", +[] (luabridge::LuaRef ref)
                    {
                        ArrStr array;
                        if (ref.isString())
                            array.push_back(ref.cast<const char *>());
                        else if (ref.isTable())
                        {
                            array.reserve(ref.length());
                            for (const auto & pair: luabridge::pairs(ref))
                            {
                                array.push_back(pair.second.cast<char>());
                            }
                        }
                        return array;
                    })
                    .addFunction("clone", &ArrStr::clone)
                    .addFunction("push_back", static_cast<bool (ArrStr::*)(const std::string &)>(&ArrStr::push_back))
                    .addFunction("pop", static_cast<char (ArrStr::*)(size_t)>(&ArrChar::pop))
                    .addFunction("front", static_cast<char (ArrStr::*)() const>(&ArrChar::front))
                    .addFunction("back", static_cast<char (ArrStr::*)() const>(&ArrChar::back))
                    .addFunction("at", static_cast<char (ArrStr::*)(size_t) const>(&ArrChar::at))
                    .addFunction("set", static_cast<void (ArrStr::*)(size_t, char)>(&ArrChar::set))
                    .addFunction("str", static_cast<std::string (ArrStr::*)() const>(&ArrChar::cpp_str))
                    .addFunction("__len", static_cast<size_t (ArrStr::*)() const>(&ArrChar::size))
                .endClass()
                /**
                 * Configurable
                 */
                .beginClass<Configurable>("Configurable")
                    .addFunction("set_conf", &LuaUtilApi::configurable_set_conf<Configurable>)
                .endClass()
                /**
                 * Service
                 */
                .beginClass<AService>("AService")
                    .addFunction("setup", &AService::setup)
                    .addFunction("init", &AService::init)
                    .addFunction("start", &AService::start)
                    .addFunction("stop", &AService::stop)
                    .addFunction("reset", &AService::reset)
                    .addFunction("is_running", &AService::is_running)
                    .addFunction("get_service_ctrl", &AService::get_service_ctrl)
                .endClass()
                .beginClass<ServiceController>("ServiceController")
                    .addFunction("get_state", &ServiceController::get_state)
                .endClass()
                /**
                 * Scheduler
                 */
                .deriveClass<Scheduler, Named>("Scheduler")
                    .addConstructor<void (*)(const std::string &, Node *), SmartNodePtr<Scheduler>>()
                    .addFunction("set_conf", &LuaUtilApi::configurable_set_conf<Scheduler>)
                    .addFunction("start", &Scheduler::start)
                    .addFunction("stop", &Scheduler::stop)
                    .addFunction("is_running", &Scheduler::is_running)
                    .addFunction("pause", &Scheduler::pause)
                    .addFunction("resume", &Scheduler::resume)
                    .addFunction("get_clock", &Scheduler::get_clock)
                    .addFunction("set_clock", &Scheduler::set_clock)
                    .addFunction("set_as_fast_as_possible", &Scheduler::set_as_fast_as_possible)
                    .addFunction("add_task", +[] (Scheduler *self, luabridge::LuaRef tbl, lua_State *state)
                    {
                        if (tbl.isTable() == false)
                            luaL_error(state, "add_task argument must be a table");

                        luabridge::LuaRef lua_fun = tbl["task"];
                        if (lua_fun.isFunction() == false)
                            luaL_error(state, "add_task table at 'task' must contain a function");

                        time_t timestamp_to_run = 0;
                        luabridge::LuaRef when = tbl["when"];
                        if (when.isNumber())
                            timestamp_to_run = when.cast<time_t>();

                        time_t reschedule_every = 0;
                        luabridge::LuaRef reschedule = tbl["reschedule"];
                        if (reschedule.isNumber())
                            reschedule_every = reschedule.cast<time_t>();
                        self->add_task(new LuaTask(lua_fun, timestamp_to_run, reschedule_every));
                    })
                    .addFunction("clear_tasks", &Scheduler::clear_tasks)
                    .addProperty("overruns", &Scheduler::overruns)
                    .addProperty("overrun_at",
                        +[] (const Scheduler *self) { return self->overrun_at; },
                        +[] (Scheduler *self, uint32_t val) { self->overrun_at = val; })
                    .addProperty("acceptable_nano",
                        +[] (const Scheduler *self) { return self->acceptable_nano; },
                        +[] (Scheduler *self, uint32_t val) { self->acceptable_nano = val; })
                .endClass()
                /**
                 * Process
                 */
                .beginClass<IRunnable>("IRunnable")
                    .addFunction("run", &IRunnable::run)
                .endClass()
                .deriveClass<IStoppableRunnable, IRunnable>("IStoppableRunnable")
                    .addFunction("is_running", &IStoppableRunnable::is_running)
                    .addFunction("stop", &IStoppableRunnable::stop)
                .endClass()
                .deriveClass<Process, IStoppableRunnable>("Process")
                    .addConstructor<void (*)()>()
                    .addFunction("add_argv", +[] (Process *self, luabridge::LuaRef ref, lua_State *state)
                    {
                        if (ref.isString())
                            self->add_argv(ref.cast<std::string>());
                        else if (ref.isTable())
                            self->add_argv(ref.cast<std::vector<std::string>>());
                        else
                            luaL_error(state, "add_argv argument must be either a string or a string table");
                    })
                    .addFunction("clear_argv", &Process::clear_argv)
                    // stdin
                    .addFunction("stdin_from", +[] (Process *self, const std::string & from)
                    {
                        self->stdin_from(from);
                    })
                    .addFunction("stdin_from_fd", +[] (Process *self, int fd)
                    {
                        self->stdin_from(fd);
                    })
                    .addFunction("stdin_from_file", &Process::stdin_from_file)
                    .addFunction("stdin_close", &Process::stdin_close)
                    // stdout
                    .addFunction("stdout_to", +[] (Process *self, luabridge::LuaRef ref, lua_State *state)
                    {
                        if (ref.isFunction() == false)
                            luaL_error(state, "stdout_to argument must a function callback");
                        self->stdout_to([ref] (const char *buf, size_t size)
                        {
                            ref(buf, size);
                        });
                    })
                    .addFunction("stdout_to_fd", +[] (Process *self, int fd)
                    {
                        self->stdout_to(fd);
                    })
                    .addFunction("stdout_to_process", +[] (Process *self, Process *another)
                    {
                        self->stdout_to(*another);
                    })
                    .addFunction("stdout_to_file", &Process::stdout_to_file)
                    .addFunction("stdout_close", &Process::stdout_close)
                    // stderr
                    .addFunction("stderr_to", +[] (Process *self, luabridge::LuaRef ref, lua_State *state)
                    {
                        if (ref.isFunction() == false)
                            luaL_error(state, "stderr_to argument must a function callback");
                        self->stderr_to([ref] (const char *buf, size_t size)
                        {
                            ref(buf, size);
                        });
                    })
                    .addFunction("stderr_to_fd", +[] (Process *self, int fd)
                    {
                        self->stderr_to(fd);
                    })
                    .addFunction("stderr_to_process", +[] (Process *self, Process *another)
                    {
                        self->stderr_to(*another);
                    })
                    .addFunction("stderr_to_file", &Process::stderr_to_file)
                    .addFunction("stderr_close", &Process::stderr_close)
                    // start - restart
                    .addFunction("start", &Process::start)
                    .addFunction("clear", &Process::clear)
                    .addFunction("reset", &Process::reset)
                    // wait
                    .addFunction("wait", &Process::wait)
                    .addFunction("wait_exit", &Process::wait_exit)
                    .addFunction("wait_stop", &Process::wait_stop)
                    .addFunction("wait_continue", &Process::wait_continue)
                    .addFunction("wait_any", &Process::wait_any)
                    // manual pipe process
                    .addFunction("read_pipes", &Process::read_pipes)
                    // end execution
                    .addFunction("end_process", &Process::end)
                    .addFunction("kill", &Process::kill)
                    // post execution
                    .addFunction("has_exited", &Process::has_exited)
                    .addFunction("has_core_dumped", &Process::has_core_dumped)
                    .addFunction("has_stopped_by_signal", &Process::has_stopped_by_signal)
                    .addFunction("has_exited_by_signal", &Process::has_exited_by_signal)
                    .addFunction("has_continued", &Process::has_continued)
                    .addFunction("signal_exit_number", &Process::signal_exit_number)
                    .addFunction("signal_stop_number", &Process::signal_stop_number)
                    .addFunction("return_code", &Process::return_code)
                    // utils
                    .addFunction("pid", &Process::pid)
                    .addFunction("argv", &Process::argv)
                .endClass()
            .endNamespace()
        .endNamespace();
}

/* ************************************************************************* */
/* LuaRunnable */
/* ************************************************************************* */

LuaUtilApi::LuaRunnable::LuaRunnable(luabridge::LuaRef lua_ref): fun(lua_ref)
{
}

LuaUtilApi::LuaRunnable::~LuaRunnable()
{
}

bool    LuaUtilApi::LuaRunnable::run()
{
    if (this->fun.isFunction())
        return this->fun();
    return false;
}

/* ************************************************************************* */
/* LuaTask */
/* ************************************************************************* */

LuaUtilApi::LuaTask::LuaTask(luabridge::LuaRef lua_ref, time_t timestamp_to_run, time_t reschedule_every):
    Task(nullptr, timestamp_to_run, reschedule_every), fun(lua_ref)
{
}

LuaUtilApi::LuaTask::~LuaTask()
{
}

bool    LuaUtilApi::LuaTask::run()
{
    if (this->fun.isFunction())
        return this->fun();
    return false;
}

}
