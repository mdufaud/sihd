#include <sihd/lua/LuaUtilApi.hpp>

#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>

#include <sihd/util/AService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Endian.hpp>
#include <sihd/util/Path.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/Types.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

#include <sihd/util/Process.hpp>

#include <sihd/util/File.hpp>
#include <sihd/util/LineReader.hpp>

#include <sihd/util/SharedMemory.hpp>

#include <unistd.h>

#define DECLARE_ARRAY_USERTYPE(ArrType, PrimitiveType)                                                                 \
 .deriveClass<ArrType, IArray>(#ArrType)                                                                               \
     .addConstructor<void (*)()>()                                                                                     \
     .addStaticFunction("new", &LuaUtilApi::_array_lua_new<PrimitiveType>)                                             \
     .addFunction("clone", &ArrType::clone)                                                                            \
     .addFunction("push_back", &LuaUtilApi::_array_lua_push_back<PrimitiveType>)                                       \
     .addFunction("push_front", &LuaUtilApi::_array_lua_push_front<PrimitiveType>)                                     \
     .addFunction("copy_from", &LuaUtilApi::_array_lua_copy_table<PrimitiveType>)                                      \
     .addFunction("from", &LuaUtilApi::_array_lua_from<PrimitiveType>)                                                 \
     .addFunction("pop", &ArrType::pop)                                                                                \
     .addFunction("front", &ArrType::front)                                                                            \
     .addFunction("back", &ArrType::back)                                                                              \
     .addFunction("at", &ArrType::at)                                                                                  \
     .addFunction("set", &ArrType::set)                                                                                \
     .addFunction("__len", &ArrType::size)                                                                             \
     .endClass()

namespace sihd::lua
{

using namespace sihd::util;

// Statics
// logger used by lua code
Logger LuaUtilApi::logger("sihd::lua");
// from path/bin/exe.lua -> path/bin -> path
std::string LuaUtilApi::dir = fs::parent(fs::parent(os::executable_path()));

SIHD_LOGGER;

bool LuaUtilApi::_configurable_recursive_set(Configurable *obj, const std::string & key, luabridge::LuaRef ref)
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
            for (const auto & key_pair : luabridge::pairs(ref))
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
            logger.error(fmt::format("Configuration key '{}' type error", key));
        }
    }
    return false;
}

void LuaUtilApi::load_process(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("util")
        /**
         * Process
         */
        .deriveClass<Process, IStoppableRunnable>("Process")
        .addConstructor<void (*)()>()
        .addFunction(
            "add_argv",
            +[](Process *self, luabridge::LuaRef ref, lua_State *state) {
                if (ref.isString())
                    self->add_argv(ref.cast<std::string>());
                else if (ref.isTable())
                    self->add_argv(ref.cast<std::vector<std::string>>());
                else
                    luaL_error(state, "add_argv argument must be either a string or a string table");
            })
        .addFunction("clear_argv", &Process::clear_argv)
        // stdin
        .addFunction(
            "stdin_from",
            +[](Process *self, const std::string & from) { self->stdin_from(from); })
        .addFunction(
            "stdin_from_fd",
            +[](Process *self, int fd) { self->stdin_from(fd); })
        .addFunction("stdin_from_file", &Process::stdin_from_file)
        .addFunction("stdin_close", &Process::stdin_close)
        // stdout
        .addFunction(
            "stdout_to",
            +[](Process *self, luabridge::LuaRef ref, lua_State *state) {
                if (ref.isFunction() == false)
                    luaL_error(state, "stdout_to argument must a function callback");
                self->stdout_to([ref](const char *buf, size_t size) {
                    try
                    {
                        ref(buf, size);
                    }
                    catch (const luabridge::LuaException & e)
                    {
                        SIHD_LOG(error, e.what());
                    }
                });
            })
        .addFunction(
            "stdout_to_fd",
            +[](Process *self, int fd) { self->stdout_to(fd); })
        .addFunction(
            "stdout_to_process",
            +[](Process *self, Process *another) { self->stdout_to(*another); })
        .addFunction("stdout_to_file", &Process::stdout_to_file)
        .addFunction("stdout_close", &Process::stdout_close)
        // stderr
        .addFunction(
            "stderr_to",
            +[](Process *self, luabridge::LuaRef ref, lua_State *state) {
                if (ref.isFunction() == false)
                    luaL_error(state, "stderr_to argument must a function callback");
                self->stderr_to([ref](const char *buf, size_t size) {
                    try
                    {
                        ref(buf, size);
                    }
                    catch (const luabridge::LuaException & e)
                    {
                        SIHD_LOG(error, e.what());
                    }
                });
            })
        .addFunction(
            "stderr_to_fd",
            +[](Process *self, int fd) { self->stderr_to(fd); })
        .addFunction(
            "stderr_to_process",
            +[](Process *self, Process *another) { self->stderr_to(*another); })
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
        .addFunction(
            "create",
            +[](SharedMemory *self, const std::string & id, size_t size, luabridge::LuaRef ref) {
                if (ref.isNil())
                    return self->create(id, size);
                return self->create(id, size, ref.cast<mode_t>());
            })
        .addFunction(
            "attach",
            +[](SharedMemory *self, const std::string & id, size_t size, luabridge::LuaRef ref) {
                if (ref.isNil())
                    return self->attach(id, size);
                return self->attach(id, size, ref.cast<mode_t>());
            })
        .addFunction(
            "attach_read_only",
            +[](SharedMemory *self, const std::string & id, size_t size, luabridge::LuaRef ref) {
                if (ref.isNil())
                    return self->attach_read_only(id, size);
                return self->attach_read_only(id, size, ref.cast<mode_t>());
            })
        .addFunction(
            "data",
            +[](SharedMemory *self) {
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

void LuaUtilApi::load_files(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("util")
        .beginNamespace("fs")
        .addFunction("exists", &fs::exists)
        .addFunction("is_file", &fs::is_file)
        .addFunction("is_dir", &fs::is_dir)
        .addFunction("filesize", &fs::filesize)
        .addFunction("remove_directory", &fs::remove_directory)
        .addFunction("remove_directories", &fs::remove_directories)
        .addFunction("make_directory", &fs::make_directory)
        .addFunction("make_directories", &fs::make_directories)
        .addFunction("children", &fs::children)
        .addFunction("recursive_children", &fs::recursive_children)
        .addFunction("is_absolute", &fs::is_absolute)
        .addFunction("normalize", &fs::normalize)
        .addFunction("trim_path", static_cast<std::string (*)(std::string_view, std::string_view)>(&fs::trim_path))
        .addFunction("parent", &fs::parent)
        .addFunction("filename", &fs::filename)
        .addFunction("extension", &fs::extension)
        .addFunction(
            "combine",
            +[](luabridge::LuaRef arg1, luabridge::LuaRef arg2) {
                if (arg1.isTable())
                {
                    std::string ret;
                    for (const auto & pair : luabridge::pairs(arg1))
                        ret = fs::combine(ret, pair.second.cast<std::string>());
                    return ret;
                }
                return fs::combine(arg1.cast<std::string>(), arg2.cast<std::string>());
            })
        .addFunction("remove_file", &fs::remove_file)
        .addFunction("are_equals", &fs::are_equals)
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
        .addFunction(
            "read",
            +[](File *self, IArray *array_ptr) { return self->read(*array_ptr); })
        .addFunction(
            "read_line",
            +[](File *self, luabridge::LuaRef ref, lua_State *state) {
                bool good = false;
                char *line = nullptr;
                size_t size = 0;
                if (ref.isNil() == false)
                    good = self->read_line_delim(&line, &size, ref.cast<std::string>()[0]);
                else
                    good = self->read_line(&line, &size);
                if (good)
                {
                    sihd::util::ArrChar array;
                    array.from(line);
                    free(line);
                    return luabridge::LuaRef(state, array);
                }
                free(line);
                return luabridge::LuaRef(state);
            })
        // write
        .addFunction(
            "write_array",
            +[](File *self, const IArray *array_ptr) { return self->write(*array_ptr); })
        .addFunction(
            "write",
            +[](File *self, const std::string & str, luabridge::LuaRef ref) {
                std::string_view view(str);
                if (ref.isNumber())
                    view.remove_suffix(ref.cast<size_t>());
                return self->write(view);
            })
        // test cases
        .addFunction("eof", &File::eof)
        .addFunction("error", &File::error)
        .addFunction("clear_errors", &File::clear_errors)
        // utils
        .addFunction("flush", &File::flush)
        .addFunction("fd", &File::fd)
        .addFunction("path", &File::path)
        .addFunction("filesize", &File::file_size)
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

void LuaUtilApi::load_threading(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("util")
        /**
         * Scheduler
         */
        .deriveClass<LuaScheduler, Named>("Scheduler")
        .addConstructor<void (*)(const std::string &, Node *), SmartNodePtr<LuaScheduler>>()
        // Configurable
        .addFunction("set_conf", &LuaUtilApi::configurable_set_conf<LuaScheduler>)
        // Scheduler
        .addFunction("stop", static_cast<bool (LuaScheduler::*)()>(&Scheduler::stop))
        .addFunction("is_running", static_cast<bool (LuaScheduler::*)() const>(&Scheduler::is_running))
        .addFunction("pause", static_cast<void (LuaScheduler::*)()>(&Scheduler::pause))
        .addFunction("resume", static_cast<void (LuaScheduler::*)()>(&Scheduler::resume))
        .addFunction("now", static_cast<time_t (LuaScheduler::*)() const>(&Scheduler::now))
        .addFunction("clock", static_cast<IClock *(LuaScheduler::*)() const>(&Scheduler::clock))
        .addFunction("set_clock", static_cast<void (LuaScheduler::*)(IClock *)>(&Scheduler::set_clock))
        .addFunction("set_as_fast_as_possible",
                     static_cast<bool (LuaScheduler::*)(bool)>(&Scheduler::set_as_fast_as_possible))
        .addFunction("clear_tasks", static_cast<void (LuaScheduler::*)()>(&Scheduler::clear_tasks))
        .addProperty(
            "overruns",
            +[](const LuaScheduler *self) { return self->overruns; })
        .addProperty(
            "overrun_at",
            +[](const LuaScheduler *self) { return self->overrun_at; },
            +[](LuaScheduler *self, uint32_t val) { self->overrun_at = val; })
        .addProperty(
            "acceptable_task_preplay_ns_time",
            +[](const LuaScheduler *self) { return self->acceptable_task_preplay_ns_time; },
            +[](LuaScheduler *self, uint32_t val) { self->acceptable_task_preplay_ns_time = val; })
        // LuaScheduler
        .addFunction("start",
                     std::function<bool(LuaScheduler *, lua_State *)>(+[](LuaScheduler *self, lua_State *state) {
                         self->set_state(state);
                         return self->start();
                     }))
        .addFunction(
            "add_task",
            +[](LuaScheduler *self, luabridge::LuaRef tbl, lua_State *state) {
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
                LuaTask *task_ptr = new LuaTask(lua_fun, timestamp_to_run, reschedule_every);
                if (task_ptr != nullptr)
                {
                    self->add_lua_task(task_ptr);
                }
            })
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
        .addFunction(
            "wait_until",
            +[](Waitable *self, time_t nano) { return self->wait_until(nano); })
        .addFunction(
            "wait_for",
            +[](Waitable *self, time_t nano) { return self->wait_for(nano); })
        .addFunction(
            "wait_for_loop",
            +[](Waitable *self, time_t nano, uint32_t times) { return self->wait_for_loop(nano, times); })
        .addFunction(
            "wait_for_elapsed",
            +[](Waitable *self, time_t nano) { return self->wait_for_elapsed(nano); })
        .addFunction("cancel_loop", &Waitable::cancel_loop)
        .endClass()
        /**
         * Waitable
         */
        .beginClass<LuaWorker>("Worker")
        .addConstructor<void (*)(luabridge::LuaRef ref)>()
        .addFunction("set_conf", &LuaUtilApi::configurable_set_conf<LuaWorker>)
        .addFunction("start_worker",
                     std::function<bool(LuaWorker *, const std::string &, lua_State *)>(
                         [&vm](LuaWorker *self, const std::string & name, lua_State *state) {
                             self->set_state(state);
                             return self->start_worker(name);
                         }))
        .addFunction("start_sync_worker",
                     std::function<bool(LuaWorker *, const std::string &, lua_State *)>(
                         [&vm](LuaWorker *self, const std::string & name, lua_State *state) {
                             self->set_state(state);
                             return self->start_sync_worker(name);
                         }))
        .addFunction("stop_worker", static_cast<bool (LuaWorker::*)()>(&Worker::stop_worker))
        .addFunction("is_worker_running", static_cast<bool (LuaWorker::*)() const>(&Worker::is_worker_running))
        .addFunction("is_worker_started", static_cast<bool (LuaWorker::*)() const>(&Worker::is_worker_started))
        .endClass()
        .beginClass<LuaStepWorker>("StepWorker")
        .addConstructor<void (*)(luabridge::LuaRef ref)>()
        .addFunction("set_conf", &LuaUtilApi::configurable_set_conf<LuaStepWorker>)
        .addFunction("start_worker",
                     std::function<bool(LuaStepWorker *, const std::string &, lua_State *)>(
                         [&vm](LuaStepWorker *self, const std::string & name, lua_State *state) {
                             self->set_state(state);
                             return self->start_worker(name);
                         }))
        .addFunction("start_sync_worker",
                     std::function<bool(LuaStepWorker *, const std::string &, lua_State *)>(
                         [&vm](LuaStepWorker *self, const std::string & name, lua_State *state) {
                             self->set_state(state);
                             return self->start_sync_worker(name);
                         }))
        .addFunction("stop_worker", static_cast<bool (LuaStepWorker::*)()>(&Worker::stop_worker))
        .addFunction("is_worker_running", static_cast<bool (LuaStepWorker::*)() const>(&Worker::is_worker_running))
        .addFunction("is_worker_started", static_cast<bool (LuaStepWorker::*)() const>(&Worker::is_worker_started))
        .addFunction("set_frequency", static_cast<bool (LuaStepWorker::*)(double)>(&StepWorker::set_frequency))
        .addFunction("pause_worker", static_cast<void (LuaStepWorker::*)()>(&StepWorker::pause_worker))
        .addFunction("resume_worker", static_cast<void (LuaStepWorker::*)()>(&StepWorker::resume_worker))
        .addFunction("nano_sleep_time", static_cast<time_t (LuaStepWorker::*)() const>(&StepWorker::nano_sleep_time))
        .addFunction("frequency", static_cast<double (LuaStepWorker::*)() const>(&StepWorker::frequency))
        .endClass()
        .endNamespace()
        .endNamespace();
}

void LuaUtilApi::load_tools(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("util")
        /**
         * Methods
         */
        .addFunction(
            "read_line",
            +[](luabridge::LuaRef ref, lua_State *state) {
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
         * Clock
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
         * Splitter
         */
        .beginClass<Splitter>("Splitter")
        .addConstructor<void (*)()>()
        .addFunction("set_empty_delimitations", &Splitter::set_empty_delimitations)
        .addFunction("set_delimiter", &Splitter::set_delimiter)
        .addFunction("set_delimiter_spaces", &Splitter::set_delimiter_spaces)
        .addFunction("set_open_escape_sequences", &Splitter::set_open_escape_sequences)
        .addFunction("set_escape_sequences_all", &Splitter::set_escape_sequences_all)
        .addFunction("split", &Splitter::split)
        .addFunction("count_tokens", &Splitter::count_tokens)
        .endClass()
        .beginClass<Timestamp>("Timestamp")
        .addConstructor<void (*)(time_t)>()
        .addFunction("get", &Timestamp::get)
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
        // convert from nano
        .addFunction("to_us", &time::to_micro)
        .addFunction("to_ms", &time::to_milli)
        .addFunction("to_sec", &time::to_sec)
        .addFunction("to_min", &time::to_min)
        .addFunction("to_hours", &time::to_hours)
        .addFunction("to_days", &time::to_days)
        .addFunction("to_double", &time::to_double)
        .addFunction("to_hz", &time::to_freq)
        // convert to nano
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
        .addFunction("id", &thread::id)
        .addFunction("id_str", static_cast<std::string (*)()>(&thread::id_str))
        .addFunction("set_name", &thread::set_name)
        .addFunction("name", &thread::name)
        .endNamespace()
        .beginNamespace("os")
        .addProperty("stdin", &LuaUtilApi::_get_int<STDIN_FILENO>)
        .addProperty("stdout", &LuaUtilApi::_get_int<STDOUT_FILENO>)
        .addProperty("stderr", &LuaUtilApi::_get_int<STDERR_FILENO>)
        .addProperty(
            "backtrace_size",
            +[]() { return os::backtrace_size; },
            +[](int val) { os::backtrace_size = val; })
        .addFunction("backtrace", &os::backtrace)
        .addFunction("pid", &os::pid)
        .addFunction("kill", &os::kill)
        .addFunction("signal_name", &os::signal_name)
        .addFunction("max_fds", &os::max_fds)
        .addFunction("is_root", &os::is_root)
        .addFunction("home_path", &os::home_path)
        .addFunction("executable_path", &os::executable_path)
        .addFunction("cwd", &os::cwd)
        .addFunction("is_run_by_debugger", &os::is_run_by_debugger)
        .addFunction("is_run_by_valgrind", &os::is_run_by_valgrind)
        .addFunction("is_run_with_asan", &os::is_run_with_asan)
        .addFunction("peak_rss", &os::peak_rss)
        .addFunction("current_rss", &os::current_rss)
        .endNamespace()
        .beginNamespace("str")
        .addFunction("timeoffset_str", &str::timeoffset_str)
        .addFunction("localtimeoffset_str", &str::localtimeoffset_str)
        .addFunction("trim", &str::trim)
        .addFunction("replace", &str::replace)
        .addFunction("starts_with", &str::starts_with)
        .addFunction("ends_with", &str::ends_with)
        .addFunction(
            "split",
            +[](const std::string & str, const std::string & delimiter) {
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
        .addFunction("is_interactive", &term::is_interactive)
        .endNamespace()
        .beginNamespace("endian")
        .addFunction("is_big", &LuaUtilApi::_is_endian<Endian::BIG>)
        .addFunction("is_little", &LuaUtilApi::_is_endian<Endian::LITTLE>)
        .endNamespace()
        .endNamespace()
        .endNamespace();
}

void LuaUtilApi::load_base(Vm & vm)
{
    if (vm.refs_exists({"sihd", "version"}))
        return;
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
        .addFunction(
            "emergency",
            +[](const std::string & log) { logger.log(emergency, log); })
        .addFunction(
            "alert",
            +[](const std::string & log) { logger.log(alert, log); })
        .addFunction(
            "critical",
            +[](const std::string & log) { logger.log(critical, log); })
        .addFunction(
            "error",
            +[](const std::string & log) { logger.log(error, log); })
        .addFunction(
            "warning",
            +[](const std::string & log) { logger.log(warning, log); })
        .addFunction(
            "notice",
            +[](const std::string & log) { logger.log(notice, log); })
        .addFunction(
            "info",
            +[](const std::string & log) { logger.log(info, log); })
        .addFunction(
            "debug",
            +[](const std::string & log) { logger.log(debug, log); })
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
        .addProperty(
            "c_ptr",
            +[](const Named *self) -> int64_t { return (int64_t)self; })
        .addFunction("parent", &Named::parent)
        .addFunction("name", &Named::name)
        .addFunction("full_name", &Named::full_name)
        .addFunction("class_name", &Named::class_name)
        .addFunction("description", &Named::description)
        .addFunction("full_name", &Named::full_name)
        .addFunction("root", &Named::root)
        .addFunction("find", static_cast<Named *(Named::*)(const std::string &)>(&Named::find))
        .addFunction("find_from", static_cast<Named *(Named::*)(Named *, const std::string &)>(&Named::find))
        .addFunction(
            "__eq",
            +[](const Named *self, const Named *other) -> bool { return self == other; })
        .endClass()
        .deriveClass<Node, Named>("Node")
        .addConstructor<void (*)(const std::string &, Node *), SmartNodePtr<Node>>()
        .addFunction("get_child", static_cast<Named *(Node::*)(const std::string &) const>(&Node::get_child))
        .addFunction(
            "add_child",
            +[](Node *self, Named *child) { return self->add_child(child); })
        .addFunction(
            "add_child_name",
            +[](Node *self, const std::string & name, Named *child) { return self->add_child(name, child); })
        .addFunction("remove_child", static_cast<bool (Node::*)(const Named *)>(&Node::remove_child))
        .addFunction("remove_child_name", static_cast<bool (Node::*)(const std::string &)>(&Node::remove_child))
        .addFunction("is_link", &Node::is_link)
        .addFunction("add_link", &Node::add_link)
        .addFunction("remove_link", &Node::remove_link)
        .addFunction("resolve_link", &Node::resolve_link)
        .addFunction("resolve_links", &Node::resolve_links)
        .addFunction("tree_str", static_cast<std::string (Node::*)() const>(&Node::tree_str))
        .addFunction("tree_desc_str", &Node::tree_desc_str)
        .addFunction("children", &Node::children)
        .addFunction("children_keys", &Node::children_keys)
        .addFunction(
            "__eq",
            +[](const Node *self, const Named *other) -> bool { return self == other; })
        .endClass()
        /**
         * Array
         */
        .beginNamespace("types")
        .addFunction("type_size", &Types::type_size)
        .addFunction("type_str", &Types::type_str)
        .addFunction("from_str", &Types::from_str)
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
        .addFunction("data_type_str", &IArray::data_type_str)
        .addFunction("hexdump", &IArray::hexdump)
        .addFunction(
            "str",
            +[](IArray *self, luabridge::LuaRef ref) {
                if (ref.isNil())
                    return self->str();
                return self->str(ref.cast<char>());
            })
        .addFunction("clear", &IArray::clear)
        // .addFunction("is_same_type", static_cast<bool (IArray::*)(const IArray &) const>(&IArray::is_same_type))
        .endClass() DECLARE_ARRAY_USERTYPE(ArrBool, bool) DECLARE_ARRAY_USERTYPE(ArrChar, char)
            DECLARE_ARRAY_USERTYPE(ArrByte, int8_t) DECLARE_ARRAY_USERTYPE(ArrUByte, uint8_t)
                DECLARE_ARRAY_USERTYPE(ArrShort, int16_t) DECLARE_ARRAY_USERTYPE(ArrUShort, uint16_t)
                    DECLARE_ARRAY_USERTYPE(ArrInt, int32_t) DECLARE_ARRAY_USERTYPE(ArrUInt, uint32_t)
                        DECLARE_ARRAY_USERTYPE(ArrLong, int64_t) DECLARE_ARRAY_USERTYPE(ArrULong, uint64_t)
                            DECLARE_ARRAY_USERTYPE(ArrFloat, float) DECLARE_ARRAY_USERTYPE(ArrDouble, double)
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
        .addFunction("service_ctrl", &AService::service_ctrl)
        .endClass()
        .beginClass<ServiceController>("ServiceController")
        .addFunction("state", &ServiceController::state)
        .endClass()
        /**
         * Runnable
         */
        .beginClass<IRunnable>("IRunnable")
        .addFunction("run", &IRunnable::run)
        .endClass()
        .deriveClass<IStoppableRunnable, IRunnable>("IStoppableRunnable")
        .addFunction("is_running", &IStoppableRunnable::is_running)
        .addFunction("stop", &IStoppableRunnable::stop)
        .endClass()
        .endNamespace()
        .endNamespace();
}

void LuaUtilApi::load_all(Vm & vm)
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
    Scheduler(name, parent),
    _state_ptr(nullptr),
    _vm_thread(nullptr)
{
}

LuaUtilApi::LuaScheduler::~LuaScheduler() {}

bool LuaUtilApi::LuaScheduler::start()
{
    if (_state_ptr == nullptr)
        return false;
    if (this->is_running())
        return true;
    {
        // create new lua stack for thread
        std::lock_guard lrun(_mutex_task);
        // make non owning LuaVm from lua_State
        Vm vm(_state_ptr);
        if (vm.new_thread(_vm_thread) == false)
            return false;
        lua_State *state_ptr = _vm_thread.lua_state();
        for (const auto & pair : _task_map)
        {
            // if lua tasks has been added before thread is created, set new lua state
            LuaTask *lua_task = dynamic_cast<LuaTask *>(pair.second);
            if (lua_task != nullptr)
                lua_task->new_lua_state(state_ptr);
        }
    }
    return Scheduler::start();
}

bool LuaUtilApi::LuaScheduler::stop()
{
    {
        std::lock_guard lrun(_mutex_task);
        _vm_thread.close_state();
    }
    return Scheduler::stop();
}

void LuaUtilApi::LuaScheduler::add_lua_task(LuaTask *task_ptr)
{
    {
        std::lock_guard l(_mutex_task);
        // if thread is running and new task is added, change lua state to thread state
        if (this->is_running() && _vm_thread.lua_state() != nullptr)
        {
            task_ptr->new_lua_state(_vm_thread.lua_state());
        }
    }
    this->add_task(task_ptr);
}

void LuaUtilApi::LuaScheduler::set_state(lua_State *state)
{
    _state_ptr = state;
}

/* ************************************************************************* */
/* LuaThreadRunner */
/* ************************************************************************* */

LuaUtilApi::LuaThreadRunner::LuaThreadRunner(luabridge::LuaRef lua_ref): _original_fun(lua_ref), _fun(lua_ref) {}

LuaUtilApi::LuaThreadRunner::~LuaThreadRunner() {}

void LuaUtilApi::LuaThreadRunner::new_lua_state(lua_State *new_state)
{
    if (_fun.state() != new_state)
    {
        // push original function on top of stack
        _original_fun.push();
        // transfer top of stack to top of thread state stack
        lua_xmove(_original_fun.state(), new_state, 1);
        // create function ref from last stack element (without index it pops the value)
        _fun = luabridge::LuaRef::fromStack(new_state, -1);
    }
}

/* ************************************************************************* */
/* LuaRunnable */
/* ************************************************************************* */

LuaUtilApi::LuaRunnable::LuaRunnable(luabridge::LuaRef lua_ref): LuaThreadRunner(lua_ref) {}

LuaUtilApi::LuaRunnable::~LuaRunnable() {}

bool LuaUtilApi::LuaRunnable::run()
{
    try
    {
        return this->call_lua_method<bool>();
    }
    catch (const luabridge::LuaException & e)
    {
        SIHD_LOG(error, e.what());
    }
    return false;
}

/* ************************************************************************* */
/* LuaTask */
/* ************************************************************************* */

LuaUtilApi::LuaTask::LuaTask(luabridge::LuaRef lua_ref, time_t timestamp_to_run, time_t reschedule_every):
    Task(nullptr, timestamp_to_run, reschedule_every),
    LuaThreadRunner(lua_ref)
{
}

LuaUtilApi::LuaTask::~LuaTask() {}

bool LuaUtilApi::LuaTask::run()
{
    try
    {
        return this->call_lua_method<bool>();
    }
    catch (const luabridge::LuaException & e)
    {
        SIHD_LOG(error, e.what());
    }
    return false;
}

/* ************************************************************************* */
/* LuaWorker */
/* ************************************************************************* */

LuaUtilApi::LuaWorker::LuaWorker(luabridge::LuaRef lua_ref): _state_ptr(nullptr), _lua_runnable(lua_ref)
{
    this->set_runnable(&_lua_runnable);
}

LuaUtilApi::LuaWorker::~LuaWorker() {}

void LuaUtilApi::LuaWorker::set_state(lua_State *state)
{
    _state_ptr = state;
}

bool LuaUtilApi::LuaWorker::start_worker(const std::string_view name)
{
    if (this->is_worker_started() == false)
    {
        // make non owning LuaVm from lua_State
        Vm current_vm(_state_ptr);
        lua_State *state = current_vm.new_luathread();
        if (state == nullptr)
            return false;
        _lua_runnable.new_lua_state(state);
    }
    return Worker::start_worker(name);
}

/* ************************************************************************* */
/* LuaStepWorker */
/* ************************************************************************* */

LuaUtilApi::LuaStepWorker::LuaStepWorker(luabridge::LuaRef lua_ref): _state_ptr(nullptr), _lua_runnable(lua_ref)
{
    this->set_runnable(&_lua_runnable);
}

LuaUtilApi::LuaStepWorker::~LuaStepWorker() {}

void LuaUtilApi::LuaStepWorker::set_state(lua_State *state)
{
    _state_ptr = state;
}

bool LuaUtilApi::LuaStepWorker::start_worker(const std::string_view name)
{
    if (this->is_worker_started() == false)
    {
        // make non owning LuaVm from lua_State
        Vm current_vm(_state_ptr);
        lua_State *state = current_vm.new_luathread();
        if (state == nullptr)
            return false;
        _lua_runnable.new_lua_state(state);
    }
    return StepWorker::start_worker(name);
}

} // namespace sihd::lua
