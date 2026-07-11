#include <unistd.h>

#include <chrono>
#include <future>

// clang-format off
#include <sihd/lua/LuaGil.hpp>
#include <sihd/lua/sys/LuaSysApi.hpp>
#include <luabridge3/LuaBridge/Vector.h>
// clang-format on
#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/FileMutex.hpp>
#include <sihd/sys/proc.hpp>
#include <sihd/sys/LineReader.hpp>
#include <sihd/sys/PathManager.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/ProcessInfo.hpp>
#include <sihd/sys/Uuid.hpp>
#include <sihd/sys/screenshot.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/build.hpp>

namespace
{
inline sihd::sys::Process::FileDescType fd_to_filedesc(int fd)
{
#if defined(__SIHD_WINDOWS__)
    return reinterpret_cast<sihd::sys::Process::FileDescType>(static_cast<intptr_t>(fd));
#else
    return static_cast<sihd::sys::Process::FileDescType>(fd);
#endif
}
} // namespace

namespace sihd::lua
{

using namespace sihd::util;
using namespace sihd::sys;
SIHD_LOGGER;

namespace
{
// from path/bin/exe.lua -> path/bin -> path
std::string g_exe_dir = fs::parent(fs::parent(fs::executable_path()));
} // namespace

/**
 * LuaProcess - Wrapper around Process with thread-safe Lua callbacks
 */
class LuaProcess: public Process
{
    public:
        LuaProcess(): Process() {}
        ~LuaProcess() = default;

        void set_stdout_callback(luabridge::LuaRef callback)
        {
            if (!callback.isFunction())
                return;
            _stdout_callback = std::make_shared<LuaSysApi::LuaProcessCallback>(callback);
            auto cb = _stdout_callback;
            this->stdout_to([cb](std::string_view data) { (*cb)(data); });
        }

        void set_stderr_callback(luabridge::LuaRef callback)
        {
            if (!callback.isFunction())
                return;
            _stderr_callback = std::make_shared<LuaSysApi::LuaProcessCallback>(callback);
            auto cb = _stderr_callback;
            this->stderr_to([cb](std::string_view data) { (*cb)(data); });
        }

        void flush_stdout()
        {
            if (_stdout_callback)
                _stdout_callback->flush();
        }

        void flush_stderr()
        {
            if (_stderr_callback)
                _stderr_callback->flush();
        }

        void flush_all()
        {
            flush_stdout();
            flush_stderr();
        }

        bool has_pending_stdout() const { return _stdout_callback && _stdout_callback->has_pending(); }

        bool has_pending_stderr() const { return _stderr_callback && _stderr_callback->has_pending(); }

    private:
        std::shared_ptr<LuaSysApi::LuaProcessCallback> _stdout_callback;
        std::shared_ptr<LuaSysApi::LuaProcessCallback> _stderr_callback;
};

/**
 * LuaProcFuture - copyable wrapper over the std::future<int> returned by proc::execute.
 * get()/wait_for() block, so the caller releases the Lua GIL around them.
 */
class LuaProcFuture
{
    public:
        explicit LuaProcFuture(std::future<int> fut):
            _future(std::make_shared<std::future<int>>(std::move(fut)))
        {
        }

        int get() { return _future->get(); }

        bool wait_for(Duration duration)
        {
            return _future->wait_for(std::chrono::nanoseconds(duration.get())) == std::future_status::ready;
        }

        bool valid() const { return _future && _future->valid(); }

    private:
        std::shared_ptr<std::future<int>> _future;
};

void LuaSysApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state()).beginNamespace("sihd").addVariable("dir", &g_exe_dir).endNamespace();

    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("sys")
        .beginClass<sihd::sys::Uuid>("Uuid")
        .addConstructor<void (*)(), void (*)(const std::string &), void (*)(const sihd::sys::Uuid &, std::string_view)>()
        .addFunction("is_null", &sihd::sys::Uuid::is_null)
        .addFunction("clear", &sihd::sys::Uuid::clear)
        .addFunction("str", &sihd::sys::Uuid::str)
        .addFunction("__tostring", &sihd::sys::Uuid::str)
        .addFunction("__eq", &sihd::sys::Uuid::operator==)
        .addStaticFunction("DNS", &sihd::sys::Uuid::DNS)
        .addStaticFunction("URL", &sihd::sys::Uuid::URL)
        .addStaticFunction("OID", &sihd::sys::Uuid::OID)
        .addStaticFunction("X500", &sihd::sys::Uuid::X500)
        .endClass()
        .beginClass<sihd::sys::ProcessInfo>("ProcessInfo")
        .addConstructor<void (*)(int), void (*)(std::string_view)>()
        .addFunction("is_alive", &sihd::sys::ProcessInfo::is_alive)
        .addFunction("pid", &sihd::sys::ProcessInfo::pid)
        .addFunction("name", +[](sihd::sys::ProcessInfo *self) -> std::string { return self->name(); })
        .addFunction("cwd", +[](sihd::sys::ProcessInfo *self) -> std::string { return self->cwd(); })
        .addFunction("exe_path", +[](sihd::sys::ProcessInfo *self) -> std::string { return self->exe_path(); })
        .addFunction("cmd_line",
                     +[](sihd::sys::ProcessInfo *self) -> std::vector<std::string> { return self->cmd_line(); })
        .addFunction("env", +[](sihd::sys::ProcessInfo *self) -> std::vector<std::string> { return self->env(); })
        .addFunction("creation_time",
                     +[](sihd::sys::ProcessInfo *self) -> int64_t { return self->creation_time(); })
        .addStaticFunction("get_all_process_from_name", &sihd::sys::ProcessInfo::get_all_process_from_name)
        .endClass()
        .beginClass<sihd::sys::Pixel>("Pixel")
        .addConstructor<void (*)(), void (*)(uint32_t)>()
        .addStaticFunction("rgb", &sihd::sys::Pixel::rgb)
        .addFunction("value", +[](sihd::sys::Pixel *self) -> uint32_t { return self->value; })
        .addFunction("red", +[](sihd::sys::Pixel *self) -> int { return self->red; })
        .addFunction("green", +[](sihd::sys::Pixel *self) -> int { return self->green; })
        .addFunction("blue", +[](sihd::sys::Pixel *self) -> int { return self->blue; })
        .addFunction("alpha", +[](sihd::sys::Pixel *self) -> int { return self->alpha; })
        .endClass()
        .beginClass<sihd::sys::Bitmap>("Bitmap")
        .addConstructor<void (*)(), void (*)(size_t, size_t, uint8_t)>()
        .addFunction("create",
                     +[](sihd::sys::Bitmap *self, int w, int h, luabridge::LuaRef bpp) {
                         self->create(static_cast<size_t>(w),
                                      static_cast<size_t>(h),
                                      bpp.isNumber() ? static_cast<uint8_t>(static_cast<int>(bpp)) : uint8_t(32));
                     })
        .addFunction("fill", &sihd::sys::Bitmap::fill)
        .addFunction("clear", &sihd::sys::Bitmap::clear)
        .addFunction("set",
                     +[](sihd::sys::Bitmap *self, int row, int line, sihd::sys::Pixel pixel) {
                         self->set(static_cast<size_t>(row), static_cast<size_t>(line), pixel);
                     })
        .addFunction("get",
                     +[](sihd::sys::Bitmap *self, int row, int line) -> sihd::sys::Pixel {
                         return self->get(static_cast<size_t>(row), static_cast<size_t>(line));
                     })
        .addFunction("is_accessible",
                     +[](sihd::sys::Bitmap *self, int row, int line) -> bool {
                         return self->is_accessible(static_cast<size_t>(row), static_cast<size_t>(line));
                     })
        .addFunction("save_bmp", +[](sihd::sys::Bitmap *self, const std::string & path) -> bool {
            return self->save_bmp(path);
        })
        .addFunction("read_bmp", +[](sihd::sys::Bitmap *self, const std::string & path) -> bool {
            return self->read_bmp(path);
        })
        .addFunction("to_bmp_data",
                     +[](sihd::sys::Bitmap *self) -> std::string {
                         auto data = self->to_bmp_data();
                         return std::string(data.begin(), data.end());
                     })
        .addFunction("read_bmp_data",
                     +[](sihd::sys::Bitmap *self, const std::string & data) -> bool {
                         return self->read_bmp_data(sihd::sys::Bitmap::Pixels(data.begin(), data.end()));
                     })
        .addFunction("empty", &sihd::sys::Bitmap::empty)
        .addFunction("width", +[](sihd::sys::Bitmap *self) -> int { return static_cast<int>(self->width()); })
        .addFunction("height", +[](sihd::sys::Bitmap *self) -> int { return static_cast<int>(self->height()); })
        .addFunction("byte_per_pixel",
                     +[](sihd::sys::Bitmap *self) -> int { return static_cast<int>(self->byte_per_pixel()); })
        .endClass()
        .beginNamespace("screenshot")
        .addFunction("supported", +[]() -> bool { return sihd::sys::screenshot::supported; })
        .addFunction("take_screen", &sihd::sys::screenshot::take_screen)
        .addFunction("take_under_cursor", &sihd::sys::screenshot::take_under_cursor)
        .addFunction("take_focused", &sihd::sys::screenshot::take_focused)
        .addFunction("take_window_name",
                     +[](sihd::sys::Bitmap & bm, const std::string & name) -> bool {
                         return sihd::sys::screenshot::take_window_name(bm, name);
                     })
        .endNamespace()
        // FileMutex: advisory inter-process file lock (no Lua stdlib equivalent)
        .beginClass<sihd::sys::FileMutex>("FileMutex")
        .addConstructor<void (*)(), void (*)(std::string_view, bool)>()
        .addFunction("lock", &sihd::sys::FileMutex::lock)
        .addFunction("try_lock", &sihd::sys::FileMutex::try_lock)
        .addFunction("unlock", &sihd::sys::FileMutex::unlock)
        .addFunction("lock_shared", &sihd::sys::FileMutex::lock_shared)
        .addFunction("try_lock_shared", &sihd::sys::FileMutex::try_lock_shared)
        .addFunction("unlock_shared", &sihd::sys::FileMutex::unlock_shared)
        .addFunction("try_lock_for",
                     +[](sihd::sys::FileMutex *self, int ms) -> bool {
                         return self->try_lock_for(std::chrono::milliseconds(ms));
                     })
        .addFunction("try_lock_shared_for",
                     +[](sihd::sys::FileMutex *self, int ms) -> bool {
                         return self->try_lock_shared_for(std::chrono::milliseconds(ms));
                     })
        .endClass()
        .endNamespace()
        .endNamespace();
}

void LuaSysApi::load_process(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("sys")
        /**
         * Process - uses LuaProcess wrapper for thread-safe callbacks
         */
        .beginClass<LuaProcess>("Process")
        .addConstructor<void (*)()>()
        .addFunction(
            "add_argv",
            +[](LuaProcess *self, luabridge::LuaRef ref, lua_State *state) {
                if (ref.isString())
                    self->add_argv(std::string(ref));
                else if (ref.isTable())
                    self->add_argv(std::vector<std::string>(ref));
                else
                    luaL_error(state, "add_argv argument must be either a string or a string table");
            })
        .addFunction("clear_argv", &LuaProcess::clear_argv)
        // stdin
        .addFunction(
            "stdin_from",
            +[](LuaProcess *self, const std::string & from) { self->stdin_from(from); })
        .addFunction(
            "stdin_from_fd",
            +[](LuaProcess *self, int fd) { self->stdin_from(fd_to_filedesc(fd)); })
        .addFunction("stdin_from_file", &LuaProcess::stdin_from_file)
        .addFunction(
            "stdin_close",
            +[](LuaProcess *self) { self->stdin_close(); })
        // stdout - thread-safe with flush
        .addFunction(
            "stdout_to",
            +[](LuaProcess *self, luabridge::LuaRef ref, lua_State *state) {
                if (ref.isFunction() == false)
                    luaL_error(state, "stdout_to argument must a function callback");
                self->set_stdout_callback(ref);
            })
        .addFunction(
            "stdout_to_fd",
            +[](LuaProcess *self, int fd) { self->stdout_to(fd_to_filedesc(fd)); })
        .addFunction(
            "stdout_to_process",
            +[](LuaProcess *self, LuaProcess *another) { self->stdout_to(*another); })
        .addFunction("stdout_to_file", &LuaProcess::stdout_to_file)
        .addFunction(
            "stdout_close",
            +[](LuaProcess *self) { self->stdout_close(); })
        .addFunction("flush_stdout", &LuaProcess::flush_stdout)
        // stderr - thread-safe with flush
        .addFunction(
            "stderr_to",
            +[](LuaProcess *self, luabridge::LuaRef ref, lua_State *state) {
                if (ref.isFunction() == false)
                    luaL_error(state, "stderr_to argument must a function callback");
                self->set_stderr_callback(ref);
            })
        .addFunction(
            "stderr_to_fd",
            +[](LuaProcess *self, int fd) { self->stderr_to(fd_to_filedesc(fd)); })
        .addFunction(
            "stderr_to_process",
            +[](LuaProcess *self, LuaProcess *another) { self->stderr_to(*another); })
        .addFunction("stderr_to_file", &LuaProcess::stderr_to_file)
        .addFunction(
            "stderr_close",
            +[](LuaProcess *self) { self->stderr_close(); })
        .addFunction("flush_stderr", &LuaProcess::flush_stderr)
        // flush all callbacks
        .addFunction("flush", &LuaProcess::flush_all)
        // start - restart
        .addFunction("execute", &LuaProcess::execute)
        .addFunction(
            "clear",
            +[](LuaProcess *self) { self->clear(); })
        .addFunction(
            "reset_proc",
            +[](LuaProcess *self) { self->reset_proc(); })
        // service
        .addFunction("start", &LuaProcess::start)
        .addFunction("stop", &LuaProcess::stop)
        .addFunction("is_running", &LuaProcess::is_running)
        // wait
        .addFunction(
            "wait",
            +[](LuaProcess *self, int options, lua_State *state) {
                sihd::lua::LuaGilRelease release(state);
                return self->wait(options);
            })
#if !defined(__SIHD_WINDOWS__)
        .addFunction(
            "wait_exit",
            +[](LuaProcess *self, int options, lua_State *state) {
                sihd::lua::LuaGilRelease release(state);
                return self->wait_exit(options);
            })
        .addFunction(
            "wait_stop",
            +[](LuaProcess *self, int options, lua_State *state) {
                sihd::lua::LuaGilRelease release(state);
                return self->wait_stop(options);
            })
        .addFunction(
            "wait_continue",
            +[](LuaProcess *self, int options, lua_State *state) {
                sihd::lua::LuaGilRelease release(state);
                return self->wait_continue(options);
            })
        .addFunction(
            "wait_any",
            +[](LuaProcess *self, int options, lua_State *state) {
                sihd::lua::LuaGilRelease release(state);
                return self->wait_any(options);
            })
#endif
        // manual pipe process
        .addFunction("read_pipes", &LuaProcess::read_pipes)
        // end execution
        .addFunction("terminate", &LuaProcess::terminate)
        .addFunction("kill", &LuaProcess::kill)
    // post execution
#if !defined(__SIHD_WINDOWS__)
        .addFunction("has_exited", &LuaProcess::has_exited)
        .addFunction("has_core_dumped", &LuaProcess::has_core_dumped)
        .addFunction("has_stopped_by_signal", &LuaProcess::has_stopped_by_signal)
        .addFunction("has_exited_by_signal", &LuaProcess::has_exited_by_signal)
        .addFunction("has_continued", &LuaProcess::has_continued)
        .addFunction("signal_exit_number", &LuaProcess::signal_exit_number)
        .addFunction("signal_stop_number", &LuaProcess::signal_stop_number)
#endif
        .addFunction("return_code", &LuaProcess::return_code)
        // utils
        .addFunction("pid", &LuaProcess::pid)
        .addFunction("argv", &LuaProcess::argv)
        .endClass()
        /**
         * ProcFuture - result of proc.execute; blocking calls release the GIL
         */
        .beginClass<LuaProcFuture>("ProcFuture")
        .addFunction(
            "get",
            +[](LuaProcFuture *self, lua_State *state) {
                sihd::lua::LuaGilRelease release(state);
                return self->get();
            })
        .addFunction(
            "wait_for",
            +[](LuaProcFuture *self, luabridge::LuaRef duration, lua_State *state) {
                Duration dur = sihd::lua::to_duration(duration);
                sihd::lua::LuaGilRelease release(state);
                return self->wait_for(dur);
            })
        .addFunction("valid", &LuaProcFuture::valid)
        .endClass()
        /**
         * proc - free-function process launcher returning a future
         */
        .beginNamespace("proc")
        .addFunction(
            "execute",
            +[](luabridge::LuaRef args, lua_State *state) {
                if (args.isTable() == false)
                    luaL_error(state, "proc.execute argument must be a string table");
                std::vector<std::string> argv(args);
                return LuaProcFuture(proc::execute(std::span<const std::string>(argv)));
            })
        .endNamespace()
        .endNamespace()
        .endNamespace();
}

void LuaSysApi::load_files(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("sys")
        .beginNamespace("fs")
        .addFunction("exists", &fs::exists)
        .addFunction("is_file", &fs::is_file)
        .addFunction("is_dir", &fs::is_dir)
        .addFunction("file_size", &fs::file_size)
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
                        ret = fs::combine(ret, std::string(pair.second));
                    return ret;
                }
                return fs::combine(std::string(arg1), std::string(arg2));
            })
        .addFunction("remove_file", &fs::remove_file)
        .addFunction("are_equals", &fs::are_equals)
        .addFunction("home_path", &fs::home_path)
        .addFunction("executable_path", &fs::executable_path)
        .addFunction("cwd", &fs::cwd)
        .addFunction("tmp_path", &fs::tmp_path)
        .addFunction("make_tmp_directory", &fs::make_tmp_directory)
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
                ssize_t ret;
                char *line = nullptr;
                size_t size = 0;
                if (ref.isNil() == false)
                    ret = self->read_line_delim(&line, &size, std::string(ref)[0]);
                else
                    ret = self->read_line(&line, &size);
                if (ret >= 0)
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
                    view.remove_suffix(static_cast<size_t>(ref));
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
        .beginClass<LineReader>("LineReader")
        .addConstructor<void (*)()>()
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

void LuaSysApi::load_tools(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("sys")
        .addFunction(
            "read_line",
            +[](lua_State *state) {
                luabridge::LuaRef ret(state);
                std::string line;
                if (LineReader::fast_read_line(line, stdin))
                    ret = line;
                return ret;
            })
        .beginNamespace("signal")
        .addFunction("kill", &signal::kill)
        .addFunction("name", &signal::name)
        .endNamespace() // signal
        .beginNamespace("os")
        .addProperty(
            "stdin",
            +[] { return STDIN_FILENO; })
        .addProperty(
            "stdout",
            +[] { return STDOUT_FILENO; })
        .addProperty(
            "stderr",
            +[] { return STDERR_FILENO; })
        .addFunction("backtrace", &sihd::sys::os::backtrace)
        .addFunction("pid", &sihd::sys::os::pid)
        .addFunction("max_fds", &sihd::sys::os::max_fds)
        .addFunction("is_root", &sihd::sys::os::is_root)
        .addFunction("is_run_by_debugger", &sihd::sys::os::is_run_by_debugger)
        .addFunction("is_run_by_valgrind", &sihd::sys::os::is_run_by_valgrind)
        .addProperty(
            "is_run_with_asan",
            +[] { return sihd::util::build::is_run_with_asan; })
        .addFunction("peak_rss", &sihd::sys::os::peak_rss)
        .addFunction("current_rss", &sihd::sys::os::current_rss)
        .endNamespace()  // os
        .endNamespace()  // sys
        .endNamespace(); // sihd
}

void LuaSysApi::load_all(Vm & vm)
{
    LuaSysApi::load_base(vm);
    LuaSysApi::load_tools(vm);
    LuaSysApi::load_files(vm);
    LuaSysApi::load_process(vm);
}

/* ************************************************************************* */
/* LuaProcessCallback */
/* ************************************************************************* */

LuaSysApi::LuaProcessCallback::LuaProcessCallback(luabridge::LuaRef callback): _callback(callback) {}

LuaSysApi::LuaProcessCallback::~LuaProcessCallback() = default;

void LuaSysApi::LuaProcessCallback::operator()(std::string_view data)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _queue.push(std::string(data));
}

void LuaSysApi::LuaProcessCallback::flush()
{
    std::queue<std::string> to_process;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::swap(to_process, _queue);
    }

    while (!to_process.empty())
    {
        try
        {
            _callback(to_process.front());
        }
        catch (const luabridge::LuaException & e)
        {
            SIHD_LOG(error, "LuaProcessCallback: {}", e.what());
        }
        to_process.pop();
    }
}

bool LuaSysApi::LuaProcessCallback::has_pending() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return !_queue.empty();
}

} // namespace sihd::lua
