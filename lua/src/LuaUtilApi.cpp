#include <sihd/lua/LuaUtilApi.hpp>

#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

#include <sihd/util/Types.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Thread.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/Str.hpp>
#include <sihd/util/Path.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Endian.hpp>

#include <sihd/util/AService.hpp>
#include <sihd/util/ServiceController.hpp>

#include <sihd/util/Process.hpp>

#include <sihd/util/File.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/SharedMemory.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Waitable.hpp>

#include <LuaBridge/Vector.h>
#include <LuaBridge/Map.h>
#include <LuaBridge/UnorderedMap.h>
#include <LuaBridge/detail/Stack.h>

#include <unistd.h>

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType) \
    .deriveClass<ArrType, IArray>(#ArrType)\
        .addConstructor<void (*)()>()\
        .addStaticFunction("new", &LuaUtilApi::_array_lua_new<PrimitiveType>)\
        .addFunction("clone", &ArrType::clone)\
        .addFunction("push_back", &LuaUtilApi::_array_lua_push_back<PrimitiveType>)\
        .addFunction("copy_from", &LuaUtilApi::_array_lua_copy_table<PrimitiveType>)\
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
    // LuaBridge smart pointer management for Named/Node pattern
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

    // std::string_view as a pushable lua class
    template <>
    struct Stack<std::string_view>
    {
        static void push(lua_State* L, const std::string_view & str)
        {
            lua_pushlstring(L, str.data(), str.size());
        }

        static std::string_view get(lua_State* L, int index)
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

        static bool isInstance(lua_State* L, int index)
        {
            return lua_type(L, index) == LUA_TSTRING;
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

    // enable enums in Lua

    template <>
    struct Stack<sihd::util::Type>: sihd::lua::EnumWrapper<sihd::util::Type>
    {
    };

    template <>
    struct Stack<sihd::util::ServiceController::State>: sihd::lua::EnumWrapper<sihd::util::ServiceController::State>
    {
    };

};

namespace sihd::lua
{

using namespace sihd::util;

Logger LuaUtilApi::logger("sihd::lua");

// from path/bin/exe.lua -> path/bin -> path
std::string LuaUtilApi::dir = Files::get_parent(Files::get_parent(OS::get_executable_path()));

SIHD_NEW_LOGGER("sihd::luapi");

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

void    LuaUtilApi::load_process(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
            .beginNamespace("util")
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
                /**
                 * SharedMemory
                 */
                .beginClass<SharedMemory>("SharedMemory")
                    .addConstructor<void (*)()>()
                    .addFunction("create", +[] (SharedMemory *self, const std::string & id, size_t size, luabridge::LuaRef ref)
                    {
                        if (ref.isNil())
                            return self->create(id, size);
                        return self->create(id, size, ref.cast<mode_t>());
                    })
                    .addFunction("attach", +[] (SharedMemory *self, const std::string & id, size_t size, luabridge::LuaRef ref)
                    {
                        if (ref.isNil())
                            return self->attach(id, size);
                        return self->attach(id, size, ref.cast<mode_t>());
                    })
                    .addFunction("attach_read_only", +[] (SharedMemory *self, const std::string & id, size_t size, luabridge::LuaRef ref)
                    {
                        if (ref.isNil())
                            return self->attach_read_only(id, size);
                        return self->attach_read_only(id, size, ref.cast<mode_t>());
                    })
                    .addFunction("data", +[] (SharedMemory *self)
                    {
                        ArrByte array;
                        if (self->data() != nullptr)
                            array.assign_bytes((uint8_t *)self->data(), self->size());
                        return array;
                    })
                    .addFunction("clear", &SharedMemory::clear)
                    .addFunction("fd", &SharedMemory::fd)
                    .addFunction("size", &SharedMemory::size)
                    .addFunction("id", &SharedMemory::id)
                    .addFunction("creator", &SharedMemory::creator)
                .endClass()
            .endNamespace()
        .endNamespace();
}

void    LuaUtilApi::load_files(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
            .beginNamespace("util")
                .beginNamespace("fs")
                    .addFunction("exists", &Files::exists)
                    .addFunction("is_file", &Files::is_file)
                    .addFunction("is_dir", &Files::is_dir)
                    .addFunction("get_filesize", &Files::get_filesize)
                    .addFunction("remove_directory", &Files::remove_directory)
                    .addFunction("remove_directories", &Files::remove_directories)
                    .addFunction("make_directory", &Files::make_directory)
                    .addFunction("make_directories", &Files::make_directories)
                    .addFunction("get_children", &Files::get_children)
                    .addFunction("get_recursive_children", &Files::get_recursive_children)
                    .addFunction("is_absolute", &Files::is_absolute)
                    .addFunction("normalize", &Files::normalize)
                    .addFunction("trim_path", static_cast<std::string (*)(const std::string &, const std::string &)>(&Files::trim_path))
                    .addFunction("get_parent", &Files::get_parent)
                    .addFunction("get_filename", &Files::get_filename)
                    .addFunction("get_extension", &Files::get_extension)
                    .addFunction("combine", +[] (luabridge::LuaRef arg1, luabridge::LuaRef arg2)
                    {
                        if (arg1.isTable())
                        {
                            std::string ret;
                            for (const auto & pair: luabridge::pairs(arg1))
                                ret = Files::combine(ret, pair.second.cast<std::string>());
                            return ret;
                        }
                        return Files::combine(arg1.cast<std::string>(), arg2.cast<std::string>());
                    })
                    .addFunction("remove_file", &Files::remove_file)
                    .addFunction("are_equals", &Files::are_equals)
                .endNamespace()
                /**
                 * File
                 */
                .beginClass<File>("File")
                    .addConstructor<void (*)()>()
                    // config
                    .addFunction("set_buffer_size", &File::set_buffer_size)
                    .addFunction("set_no_buffering", &File::set_no_buffering)
                    .addFunction("set_buffering_line", &File::set_buffering_line)
                    .addFunction("set_buffering_full", &File::set_buffering_full)
                    .addFunction("buff_stream", &File::buff_stream)
                    // open
                    .addFunction("open", &File::open)
                    .addFunction("is_open", &File::is_open)
                    .addFunction("close", &File::close)
                    // read
                    .addFunction("read", +[] (File *self, IArray *array_ptr)
                    {
                        return self->read(*array_ptr);
                    })
                    .addFunction("read_line", +[] (File *self, luabridge::LuaRef ref, lua_State *state)
                    {
                        bool good = false;
                        char *line = nullptr;
                        size_t size = 0;
                        if (ref.isNil() == false)
                            good = self->read_line_delim(&line, &size, ref.cast<std::string>()[0]);
                        else
                            good = self->read_line(&line, &size);
                        if (good)
                        {
                            sihd::util::ArrStr array;
                            array.from(line);
                            free(line);
                            return luabridge::LuaRef(state, array);
                        }
                        free(line);
                        return luabridge::LuaRef(state);
                    })
                    // write
                    .addFunction("write_array", +[] (File *self, const IArray *array_ptr, luabridge::LuaRef ref)
                    {
                        size_t byte_offset = 0;
                        if (ref.isNumber())
                            byte_offset = ref.cast<size_t>();
                        return self->write(*array_ptr, byte_offset);
                    })
                    .addFunction("write", +[] (File *self, const std::string & str, luabridge::LuaRef ref)
                    {
                        size_t size_limit = 0;
                        if (ref.isNumber())
                            size_limit = ref.cast<size_t>();
                        return self->write(str, size_limit);
                    })
                    // test cases
                    .addFunction("eof", &File::eof)
                    .addFunction("error", &File::error)
                    .addFunction("clear_errors", &File::clear_errors)
                    // utils
                    .addFunction("flush", &File::flush)
                    .addFunction("fd", &File::fd)
                    .addFunction("path", &File::path)
                    .addFunction("filesize", &File::filesize)
                    // lock
                    .addFunction("lock", &File::lock)
                    .addFunction("trylock", &File::trylock)
                    .addFunction("unlock", &File::unlock)
                .endClass()
                /**
                 * LineReader
                 */
                .deriveClass<LineReader, Named>("LineReader")
                    .addConstructor<void (*)(const std::string &, Node *), SmartNodePtr<LineReader>>()
                    // Configurable
                    .addFunction("set_conf", &LuaUtilApi::configurable_set_conf<LineReader>)
                    // IReader
                    .addFunction("read_next", &LineReader::read_next)
                    .addFunction("get_read_data", &LuaUtilApi::ireader_get_read_data<LineReader>)
                    // other
                    .addFunction("open", &LineReader::open)
                    .addFunction("is_open", &LineReader::is_open)
                    .addFunction("close", &LineReader::close)
                    .addFunction("error", &LineReader::error)
                    .addFunction("buffsize", &LineReader::buffsize)
                    .addFunction("line_buffsize", &LineReader::line_buffsize)
                .endClass()
            .endNamespace()
        .endNamespace();
}

void    LuaUtilApi::load_threading(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
            .beginNamespace("util")
                /**
                 * Scheduler
                 */
                .deriveClass<Scheduler, Named>("CppScheduler")
                    // .addConstructor<void (*)(const std::string &, Node *), SmartNodePtr<Scheduler>>()
                    // Configurable
                    .addFunction("set_conf", &LuaUtilApi::configurable_set_conf<Scheduler>)
                    // other
                    .addFunction("start", &Scheduler::start)
                    .addFunction("stop", &Scheduler::stop)
                    .addFunction("is_running", &Scheduler::is_running)
                    .addFunction("pause", &Scheduler::pause)
                    .addFunction("resume", &Scheduler::resume)
                    .addFunction("get_clock", &Scheduler::get_clock)
                    .addFunction("set_clock", &Scheduler::set_clock)
                    .addFunction("set_as_fast_as_possible", &Scheduler::set_as_fast_as_possible)
                    .addFunction("clear_tasks", &Scheduler::clear_tasks)
                    .addProperty("overruns", &Scheduler::overruns)
                    .addProperty("overrun_at",
                        +[] (const Scheduler *self) { return self->overrun_at; },
                        +[] (Scheduler *self, uint32_t val) { self->overrun_at = val; })
                    .addProperty("acceptable_nano",
                        +[] (const Scheduler *self) { return self->acceptable_nano; },
                        +[] (Scheduler *self, uint32_t val) { self->acceptable_nano = val; })
                .endClass()
                .deriveClass<LuaScheduler, Scheduler>("Scheduler")
                    .addConstructor<void (*)(const std::string &, Node *), SmartNodePtr<LuaScheduler>>()
                    .addFunction("start", std::function<bool (LuaScheduler *)>([&vm] (LuaScheduler *self)
                    {
                        self->set_vm(&vm);
                        return self->start();
                    }))
                    .addFunction("add_task", +[] (LuaScheduler *self, luabridge::LuaRef tbl, lua_State *state)
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
                        LuaTask *task_ptr = new LuaTask(self, lua_fun, timestamp_to_run, reschedule_every);
                        if (task_ptr != nullptr)
                            self->add_task(task_ptr);
                    })
                .endClass()
                /**
                 * Splitter
                 */
                .beginClass<Splitter>("Splitter")
                    .addConstructor<void (*)()>()
                    .addFunction("set_empty_delimitations", &Splitter::set_empty_delimitations)
                    .addFunction("set_delimiter", &Splitter::set_delimiter)
                    .addFunction("set_delimiter_spaces", &Splitter::set_delimiter_spaces)
                    .addFunction("set_escape_sequences", &Splitter::set_escape_sequences)
                    .addFunction("set_escape_sequences_all", &Splitter::set_escape_sequences_all)
                    .addFunction("split", &Splitter::split)
                    .addFunction("count_tokens", &Splitter::count_tokens)
                .endClass()
                /**
                 * Waitable
                 */
                .beginClass<Waitable>("Waitable")
                    .addConstructor<void (*)()>()
                    .addFunction("notify", &Waitable::notify)
                    .addFunction("notify_all", &Waitable::notify_all)
                    .addFunction("infinite_wait", &Waitable::infinite_wait)
                    .addFunction("infinite_wait_elapsed", &Waitable::infinite_wait_elapsed)
                    .addFunction("wait_until", &Waitable::wait_until)
                    .addFunction("wait_for", &Waitable::wait_for)
                    .addFunction("wait_loop", &Waitable::wait_loop)
                    .addFunction("cancel_loop", &Waitable::cancel_loop)
                    .addFunction("wait_elapsed", &Waitable::wait_elapsed)
                .endClass()
            .endNamespace()
        .endNamespace();
}

void    LuaUtilApi::load_tools(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
            .beginNamespace("util")
                /**
                 * Methods
                 */
                .addFunction("read_line", +[] (luabridge::LuaRef ref, lua_State *state)
                {
                    luabridge::LuaRef ret(state);
                    size_t buffsize = 1;
                    if (ref.isNumber())
                        buffsize = ref.cast<size_t>();
                    std::string line;
                    if (LineReader::fast_read_line(line, stdin, buffsize))
                        ret = line;
                    return ret;
                })
                /**
                 * Properties
                 */
                .addProperty("platform", &LuaUtilApi::_get_platform_str)
                .addVariable("clock", &Clock::default_clock)
                /**
                 * Classes
                 */
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
                /**
                 * Namespaces
                 */
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
                .beginNamespace("str")
                    .addFunction("gmtime_to_string", &Str::gmtime_to_string)
                    .addFunction("localtime_to_string", &Str::localtime_to_string)
                    .addFunction("trim", &Str::trim)
                    .addFunction("replace", &Str::replace)
                    .addFunction("starts_with", &Str::starts_with)
                    .addFunction("ends_with", &Str::ends_with)
                    .addFunction("split", +[] (const std::string & str, const std::string & delimiter)
                    {
                        Splitter splitter(delimiter);
                        return splitter.split(str);
                    })
                .endNamespace()
                .beginNamespace("path")
                    .addFunction("set", &Path::set)
                    .addFunction("clear", &Path::clear)
                    .addFunction("clear_all", &Path::clear_all)
                    .addFunction("get", static_cast<std::string (*)(const std::string &)>(&Path::get))
                    .addFunction("get_from_url", static_cast<std::string (*)(const std::string &, const std::string &)>(&Path::get))
                    .addFunction("get_from_path", &Path::get_from)
                    .addFunction("find", &Path::find)
                .endNamespace()
                .beginNamespace("term")
                    .addFunction("is_interactive", &Term::is_interactive)
                .endNamespace()
                .beginNamespace("endian")
                    .addFunction("is_big", &LuaUtilApi::_is_endian<Endian::BIG>)
                    .addFunction("is_little", &LuaUtilApi::_is_endian<Endian::LITTLE>)
                .endNamespace()
            .endNamespace()
        .endNamespace();
}

void    LuaUtilApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
            .addProperty("dir", &LuaUtilApi::dir, false)
            .addProperty("version", &LuaUtilApi::_get_version_str)
            .addProperty("version_num", &LuaUtilApi::_get_int<SIHD_VERSION_NUM>)
            .addProperty("version_major", &LuaUtilApi::_get_int<SIHD_VERSION_MAJOR>)
            .addProperty("version_minor", &LuaUtilApi::_get_int<SIHD_VERSION_MINOR>)
            .addProperty("version_patch", &LuaUtilApi::_get_int<SIHD_VERSION_PATCH>)
            .beginNamespace("util")
                .beginNamespace("log")
                    .addFunction("debug", +[] (const std::string & log) { logger.log(debug, log); })
                    .addFunction("info", +[] (const std::string & log) { logger.log(info, log); })
                    .addFunction("warning", +[] (const std::string & log) { logger.log(warning, log); })
                    .addFunction("error", +[] (const std::string & log) { logger.log(error, log); })
                    .addFunction("critical", +[] (const std::string & log) { logger.log(critical, log); })
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
                .beginNamespace("types")
                    .addFunction("type_size", &Types::type_size)
                    .addFunction("type_to_string", &Types::type_to_string)
                    .addFunction("string_to_type", &Types::string_to_type)
                .endNamespace()
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
                    .addStaticFunction("new", +[] (luabridge::LuaRef ref, lua_State *state)
                    {
                        ArrStr array;
                        if (ref.isString())
                            array.push_back(ref.cast<const char *>());
                        else
                            luaL_error(state, "ArrStr new argument must be a string");
                        return array;
                    })
                    .addFunction("push_back", static_cast<bool (ArrStr::*)(const std::string &)>(&ArrStr::push_back))
                    .addFunction("copy_from", +[] (ArrStr *self, const std::string & src, luabridge::LuaRef from_ref)
                    {
                        size_t from = 0;
                        if (from_ref.isNil() == false)
                            from = from_ref.cast<size_t>();
                        return self->copy_from(src, from);
                    })
                    .addFunction("clone", &ArrStr::clone)
                    .addFunction("pop", static_cast<char (ArrStr::*)(size_t)>(&ArrChar::pop))
                    .addFunction("front", static_cast<char (ArrStr::*)() const>(&ArrChar::front))
                    .addFunction("back", static_cast<char (ArrStr::*)() const>(&ArrChar::back))
                    .addFunction("at", static_cast<char (ArrStr::*)(size_t) const>(&ArrChar::at))
                    .addFunction("set", static_cast<void (ArrStr::*)(size_t, char)>(&ArrChar::set))
                    .addFunction("str", static_cast<std::string (ArrStr::*)() const>(&ArrChar::cpp_str))
                    .addFunction("__len", static_cast<size_t (ArrStr::*)() const>(&ArrChar::size))
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
            .endNamespace()
        .endNamespace();
}

void    LuaUtilApi::load_all(Vm & vm)
{
    LuaUtilApi::load_base(vm);
    LuaUtilApi::load_tools(vm);
    LuaUtilApi::load_threading(vm);
    LuaUtilApi::load_files(vm);
    LuaUtilApi::load_process(vm);
}

/* ************************************************************************* */
/* LuaScheduler */
/* ************************************************************************* */

LuaUtilApi::LuaScheduler::LuaScheduler(const std::string & name, sihd::util::Node *parent):
    Scheduler(name, parent)
{
    _vm_ptr = nullptr;
    _state_ptr = nullptr;
}

LuaUtilApi::LuaScheduler::~LuaScheduler()
{
}

bool    LuaUtilApi::LuaScheduler::run()
{
    bool ret = false;
    if (_vm_ptr != nullptr)
    {
        _state_ptr = _vm_ptr->new_thread();
        ret = Scheduler::run();
        _state_ptr = nullptr;
    }
    return ret;
}

void    LuaUtilApi::LuaScheduler::set_vm(Vm *vm_ptr)
{
    _vm_ptr = vm_ptr;
}

lua_State   *LuaUtilApi::LuaScheduler::lua_state() const
{
    return _state_ptr;
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

LuaUtilApi::LuaTask::LuaTask(ILuaThreadStateHandler *handler,
                                luabridge::LuaRef lua_ref,
                                time_t timestamp_to_run,
                                time_t reschedule_every):
    Task(nullptr, timestamp_to_run, reschedule_every), _lua_thread_handler_ptr(handler), _fun(lua_ref)
{
}

LuaUtilApi::LuaTask::~LuaTask()
{
}

bool    LuaUtilApi::LuaTask::run()
{
    if (_fun.isFunction())
    {
        _fun.push();
        lua_xmove(_fun.state(), _lua_thread_handler_ptr->lua_state(), 1);
        lua_call(_lua_thread_handler_ptr->lua_state(), 0, 0);
        return true;
    }
    return false;
}

}
