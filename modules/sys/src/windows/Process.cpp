#include <sihd/sys/platform.hpp>

#include <cerrno>
#include <cstdio>
#include <stdexcept>

#ifndef SIHD_PROCESS_READ_BUFFER_SIZE
# define SIHD_PROCESS_READ_BUFFER_SIZE 2048
#endif

#ifndef SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE
# define SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE 0740
#endif

#include <sihd/sys/File.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

extern "C"
{
extern char **environ;
}

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

/**
 * Functions helper
 */

namespace
{

void safe_close(HANDLE & fd)
{
    if (fd == nullptr)
        return;
    if (!CloseHandle(fd))
    {
        SIHD_LOG(error, "Process: could not close fd: {}", os::last_error_str());
    }
    else
    {
        fd = nullptr;
    }
}

std::pair<HANDLE, HANDLE> make_pipe()
{
    HANDLE rd;
    HANDLE rw;
    SECURITY_ATTRIBUTES saAttr;

    // locked-down default: neither end is inherited by a child and the read end is
    // non-blocking (the parent peeks it). Each call site opts a specific end into
    // inheritance / blocking when it actually needs it.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = FALSE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&rd, &rw, &saAttr, 0))
        throw std::runtime_error("Cannot make pipe: CreatePipe");

    DWORD mode = PIPE_NOWAIT;
    if (!SetNamedPipeHandleState(rd, &mode, NULL, NULL))
    {
        CloseHandle(rd);
        CloseHandle(rw);
        throw std::runtime_error("Cannot make pipe: SetNamedPipeHandleState");
    }

    // read - write;
    return std::make_pair(rd, rw);
}

// the read end is non-blocking by default; a child that reads it as its stdin needs it blocking
void make_read_blocking(HANDLE fd_read)
{
    DWORD mode = PIPE_WAIT;
    SetNamedPipeHandleState(fd_read, &mode, NULL, NULL);
}

// opt an end into inheritance so the child receives it (read end = its stdin, write end = its stdout/stderr)
void make_read_inheritable(HANDLE fd_read)
{
    SetHandleInformation(fd_read, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
}

void make_write_inheritable(HANDLE fd_write)
{
    SetHandleInformation(fd_write, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
}

bool read_pipe_into_callback(HANDLE fd, std::function<void(std::string_view)> fun)
{
    DWORD total_bytes_avail;
    DWORD bytes_left;

    if (!PeekNamedPipe(fd, NULL, 0, NULL, &total_bytes_avail, &bytes_left))
        return false;
    if (total_bytes_avail <= 0)
        return false;

    char buffer[SIHD_PROCESS_READ_BUFFER_SIZE + 1];
    DWORD read;
    BOOL success = ReadFile(fd, &buffer, SIHD_PROCESS_READ_BUFFER_SIZE, &read, NULL);
    if (success)
    {
        buffer[read] = 0;
        fun(std::string_view {buffer, (size_t)read});
    }
    return success;
}

bool write_into_pipe(HANDLE fd, const std::string & str)
{
    DWORD written;
    BOOL success = WriteFile(fd, str.c_str(), str.size(), &written, NULL);
    return success && written == (ssize_t)str.size();
}

bool read_pipe_into_file(HANDLE fd, const std::string & path, bool append)
{
    File file(path, append ? "a" : "w");

    if (!file.is_open())
        return false;

    auto fun = [&file](std::string_view buffer) {
        file.write(buffer.data(), buffer.size());
    };
    return read_pipe_into_callback(fd, fun);
}

} // namespace

/**
 * Wrapper PIPE
 */

namespace
{

enum FileDescAction
{
    None,
    File,
    FileAppend,
    Close,
};

struct StdFdWrapper
{
        HANDLE fd_read = nullptr;
        HANDLE fd_write = nullptr;
        FileDescAction action = None;
        std::function<void(std::string_view)> fun;
        std::string path;

        // fd utilities
        void add_pipe();
        // process fds once child process executed
        bool process_read_pipe();

        // fd redirections setting
        void close();
        void redirect_to(std::function<void(std::string_view)> && fun);
        void redirect_to(std::string & output);
        void redirect_to(Process::FileDescType fd);
        bool redirect_to_file(std::string_view path, bool append, mode_t open_mode);
        void reset();
        void zero_fd_read();
};

void StdFdWrapper::zero_fd_read()
{
    this->fd_read = nullptr;
}

void StdFdWrapper::add_pipe()
{
    if (this->fd_write != nullptr)
        return;
    auto [fd_read, fd_write] = make_pipe();
    this->fd_read = fd_read;
    this->fd_write = fd_write;
}

bool StdFdWrapper::process_read_pipe()
{
    if (this->fd_read == nullptr)
        return true;
    if (this->action == File || this->action == FileAppend)
        return read_pipe_into_file(this->fd_read, this->path, this->action == FileAppend);
    else if (this->fun)
        return read_pipe_into_callback(this->fd_read, this->fun);
    return true;
}

void StdFdWrapper::reset()
{
    this->action = None;
    this->fun = nullptr;
    this->path.clear();
}

void StdFdWrapper::close()
{
    this->action = Close;
    safe_close(this->fd_read);
    safe_close(this->fd_write);
    this->fun = nullptr;
}

void StdFdWrapper::redirect_to(std::function<void(std::string_view)> && fun)
{
    this->add_pipe();
    // child writes its stdout/stderr into the pipe; the parent peeks the read end (left non-blocking)
    make_write_inheritable(this->fd_write);
    this->fun = std::move(fun);
}

void StdFdWrapper::redirect_to(std::string & output)
{
    this->add_pipe();
    make_write_inheritable(this->fd_write);
    this->fun = [&output](std::string_view buffer) {
        output.append(buffer);
    };
}

void StdFdWrapper::redirect_to(Process::FileDescType fd)
{
    this->fun = nullptr;
    this->fd_write = fd;
}

bool StdFdWrapper::redirect_to_file(std::string_view path, bool append, mode_t open_mode)
{
# pragma message("TODO CreateFile permissions in windows")
    (void)open_mode;
    this->fun = nullptr;
    SECURITY_ATTRIBUTES sec_attr;
    sec_attr.nLength = sizeof(sec_attr);
    sec_attr.lpSecurityDescriptor = NULL;
    sec_attr.bInheritHandle = TRUE;
    this->fd_write = CreateFile(std::string(path).c_str(),
                                GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                &sec_attr,
                                (append ? OPEN_ALWAYS : CREATE_ALWAYS),
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
    const bool success = this->fd_write != nullptr;
    if (success)
        this->action = append ? FileAppend : File;
    else
        SIHD_LOG(error, "Process: could not open output file: {}", path);
    return success;
}

struct PipeWrapper
{
        StdFdWrapper std_in;
        StdFdWrapper std_out;
        StdFdWrapper std_err;

        void reset();
};

void PipeWrapper::reset()
{
    std_in.reset();
    std_out.reset();
    std_err.reset();
}

struct ProcessWatcher
{
        std::mutex mutex;
        DWORD code;
        bool exited;
        PROCESS_INFORMATION procinfo;

        void reset();
        bool has_terminated();
        void check_status(int options);
        Process::ReturnCodeType return_code();
};

void ProcessWatcher::check_status(int options)
{
    std::lock_guard l(this->mutex);
    const DWORD timeout_ms = options == 0 ? INFINITE : options;
    const DWORD result = WaitForSingleObject(this->procinfo.hProcess, timeout_ms);
    if (result == WAIT_OBJECT_0)
    {
        // The child process was terminated
        const BOOL got_code = GetExitCodeProcess(this->procinfo.hProcess, &this->code);
        if (got_code)
        {
            this->exited = true;

            if (this->procinfo.hProcess != nullptr)
                CloseHandle(this->procinfo.hProcess);
            this->procinfo.hProcess = nullptr;

            if (this->procinfo.hThread != nullptr)
                CloseHandle(this->procinfo.hThread);
            this->procinfo.hThread = nullptr;
        }
        else
        {
            SIHD_LOG_ERROR("Process: GetExitCodeProcess: {}", os::last_error_str());
        }
    }
    else if (result == WAIT_TIMEOUT)
    {
        // The child process is still running
        this->exited = false;
    }
    else
    {
        SIHD_LOG_ERROR("Process: WaitForSingleObject: {}", os::last_error_str());
    }
}

bool ProcessWatcher::has_terminated()
{
    std::lock_guard l(this->mutex);
    return this->exited;
}

void ProcessWatcher::reset()
{
    this->code = -1;
    this->exited = false;
    if (this->procinfo.hProcess != nullptr)
        CloseHandle(this->procinfo.hProcess);
    if (this->procinfo.hThread != nullptr)
        CloseHandle(this->procinfo.hThread);
    ZeroMemory(&this->procinfo, sizeof(PROCESS_INFORMATION));
}

Process::ReturnCodeType ProcessWatcher::return_code()
{
    return this->exited ? this->code : -1;
}

} // namespace

struct Process::Impl
{
        PipeWrapper pipe;
        ProcessWatcher process_watcher;
};

Process::Process()
{
    _impl = std::make_unique<Impl>();
    _impl->pipe.reset();
    _impl->process_watcher.reset();

    _started.store(false);
    _executing = false;
    _force_fork = false;
    _close_stdin_after_exec = false;
    _poll.set_timeout(5);
    _poll.add_observer(this);
    _poll.set_service_wait_stop(true);

    this->open_mode = SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE;

    this->clear();
}

Process::Process(std::function<int()> fun): Process()
{
    _fun_to_execute = fun;
}

Process::Process(std::span<const std::string> args): Process()
{
    _argv = std::vector<std::string>(args.begin(), args.end());
}

Process::Process(std::span<std::string_view> args): Process()
{
    _argv = std::vector<std::string>(args.begin(), args.end());
}

Process::Process(std::initializer_list<std::string_view> args): Process()
{
    _argv = std::vector<std::string>(args.begin(), args.end());
}

Process::Process(std::span<const char *> args): Process()
{
    _argv.reserve(args.size() + 1);
    for (const char *arg : args)
        _argv.emplace_back(arg);
}

Process::~Process()
{
    this->terminate();
    if (this->is_running())
    {
        this->stop();
        this->service_wait_stop();
    }
}

void Process::reset_proc()
{
    this->terminate();

    _impl->process_watcher.reset();
    _started.store(false);
}

void Process::clear()
{
    this->env_clear();
    this->env_load(str::table_span(environ));
    this->reset_proc();
    _impl->pipe.reset();
}

// Pipe stdin

Process & Process::stdin_close()
{
    _impl->pipe.std_in.close();
    return *this;
}

Process & Process::stdin_from(const std::string & input)
{
    _impl->pipe.std_in.add_pipe();
    // child reads this end as its stdin: it must be inherited and blocking (the parent keeps the write end)
    make_read_inheritable(_impl->pipe.std_in.fd_read);
    make_read_blocking(_impl->pipe.std_in.fd_read);
    write_into_pipe(_impl->pipe.std_in.fd_write, input);
    return *this;
}

Process & Process::stdin_from(FileDescType fd)
{
    _impl->pipe.std_in.fd_read = fd;
    return *this;
}

bool Process::stdin_from_file(std::string_view path)
{
    SECURITY_ATTRIBUTES sec_attr;
    sec_attr.nLength = sizeof(sec_attr);
    sec_attr.lpSecurityDescriptor = NULL;
    sec_attr.bInheritHandle = TRUE;
    _impl->pipe.std_in.fd_read = CreateFile(std::string(path).c_str(),
                                            GENERIC_READ,
                                            FILE_SHARE_READ,
                                            &sec_attr,
                                            OPEN_EXISTING,
                                            FILE_ATTRIBUTE_NORMAL,
                                            NULL);
    bool success = _impl->pipe.std_in.fd_read != INVALID_HANDLE_VALUE;
    if (!success)
        _impl->pipe.std_in.fd_read = nullptr;
    if (!success)
        SIHD_LOG(error, "Process: could not open file input: {}", path);
    return success;
}

// Pipe stdout

Process & Process::stdout_close()
{
    _impl->pipe.std_out.close();
    return *this;
}

Process & Process::stdout_to(std::function<void(std::string_view)> fun)
{
    _impl->pipe.std_out.redirect_to(std::move(fun));
    return *this;
}

Process & Process::stdout_to(std::string & output)
{
    _impl->pipe.std_out.redirect_to(output);
    return *this;
}

Process & Process::stdout_to(FileDescType fd)
{
    _impl->pipe.std_out.redirect_to(fd);
    return *this;
}

Process & Process::stdout_to(Process & proc)
{
    _impl->pipe.std_out.add_pipe();
    // both ends go to children: this child writes its stdout (write end), proc reads it as its
    // stdin (read end), so both must be inherited and the read end must be blocking
    make_write_inheritable(_impl->pipe.std_out.fd_write);
    make_read_inheritable(_impl->pipe.std_out.fd_read);
    make_read_blocking(_impl->pipe.std_out.fd_read);
    proc.stdin_from(_impl->pipe.std_out.fd_read);
    _impl->pipe.std_out.zero_fd_read();
    return *this;
}

bool Process::stdout_to_file(std::string_view path, bool append)
{
    return _impl->pipe.std_out.redirect_to_file(path, append, this->open_mode);
}

// Pipe stderr

Process & Process::stderr_close()
{
    _impl->pipe.std_err.close();
    return *this;
}

Process & Process::stderr_to(std::function<void(std::string_view)> fun)
{
    _impl->pipe.std_err.redirect_to(std::move(fun));
    return *this;
}

Process & Process::stderr_to(std::string & output)
{
    _impl->pipe.std_err.redirect_to(output);
    return *this;
}

Process & Process::stderr_to(FileDescType fd)
{
    _impl->pipe.std_err.redirect_to(fd);
    return *this;
}

Process & Process::stderr_to(Process & proc)
{
    _impl->pipe.std_err.add_pipe();
    // both ends go to children: this child writes its stderr (write end), proc reads it as its
    // stdin (read end), so both must be inherited and the read end must be blocking
    make_write_inheritable(_impl->pipe.std_err.fd_write);
    make_read_inheritable(_impl->pipe.std_err.fd_read);
    make_read_blocking(_impl->pipe.std_err.fd_read);
    proc.stdin_from(_impl->pipe.std_err.fd_read);
    _impl->pipe.std_err.zero_fd_read();
    return *this;
}

bool Process::stderr_to_file(std::string_view path, bool append)
{
    return _impl->pipe.std_err.redirect_to_file(path, append, this->open_mode);
}

// Execution

bool Process::_do_fork(const std::vector<const char *> &, const std::vector<const char *> &)
{
    return false;
}

bool Process::_do_spawn(const std::vector<const char *> &, const std::vector<const char *> &)
{
    return false;
}

bool Process::_do_child_process(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
    if (_fun_to_execute)
    {
        SIHD_LOG(error, "Process: set_function is not supported on windows (no fork)");
        return false;
    }

    STARTUPINFO start_info;
    BOOL success = FALSE;

    // Set up members of the STARTUPINFO structure.
    ZeroMemory(&start_info, sizeof(STARTUPINFO));
    start_info.cb = sizeof(STARTUPINFO);

    if (_impl->pipe.std_in.fd_read != nullptr)
        start_info.hStdInput = _impl->pipe.std_in.fd_read;
    else if (_impl->pipe.std_in.action != Close)
        start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    if (_impl->pipe.std_out.fd_write != nullptr)
        start_info.hStdOutput = _impl->pipe.std_out.fd_write;
    else if (_impl->pipe.std_out.action != Close)
        start_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    if (_impl->pipe.std_err.fd_write != nullptr)
        start_info.hStdError = _impl->pipe.std_err.fd_write;
    else if (_impl->pipe.std_err.action != Close)
        start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    start_info.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmd_line;
    for (const char *arg : argv)
    {
        if (arg == nullptr)
            break;
        if (!cmd_line.empty())
            cmd_line += " ";
        cmd_line += arg;
    }

    // CreateProcess wants a double-null-terminated block of null-separated strings
    std::string env_str;
    for (const char *val : env)
    {
        if (val == nullptr)
            break;
        env_str += val;
        env_str.push_back('\0');
    }
    env_str.push_back('\0');

    // Create the child process.

    // const DWORD creation_flags = NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW;
    const DWORD creation_flags = 0;

    success = CreateProcess(NULL,
                            cmd_line.data(), // command line
                            NULL,            // process security attributes
                            NULL,            // primary thread security attributes
                            TRUE,            // handles are inherited
                            creation_flags,  // creation flags
                            env_str.empty() ? NULL : (LPVOID)env_str.data(), // environment
                            _chdir.empty() ? NULL : _chdir.data(),           // current directory
                            &start_info,                                     // STARTUPINFO pointer
                            &_impl->process_watcher.procinfo);               // receives PROCESS_INFORMATION

    // If an error occurs, exit the application.
    if (!success)
    {
        SIHD_LOG(error, "Process: {}", os::last_error_str());
        return false;
    }
    else
    {
        // Close handles to the child process and its primary thread.
        // Some applications might keep these handles to monitor the status
        // of the child process
    }
    return success;
}

bool Process::_do_execute(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
    return this->_do_child_process(argv, env);
}

bool Process::execute()
{
    if (this->is_process_running() || _executing.exchange(true) == true)
        return false;

    Defer d([this] { _executing.store(false); });

    if (!_fun_to_execute && _argv.size() == 0)
    {
        SIHD_LOG(error, "Process: Could not run process with no arguments");
        return false;
    }

    std::vector<const char *> c_argv;
    c_argv.reserve(_argv.size() + 1);
    for (const std::string & arg : _argv)
    {
        c_argv.emplace_back(arg.c_str());
    }
    c_argv.emplace_back(nullptr);

    std::vector<const char *> c_environ;
    c_environ.reserve(_environment.size() + 1);
    for (const std::string & env : _environment)
    {
        c_environ.emplace_back(env.c_str());
    }
    c_environ.emplace_back(nullptr);

    const bool success = this->_do_execute(c_argv, c_environ);
    if (success)
    {
        safe_close(_impl->pipe.std_in.fd_read);
        safe_close(_impl->pipe.std_out.fd_write);
        safe_close(_impl->pipe.std_err.fd_write);
    }

    if (_close_stdin_after_exec)
    {
        safe_close(_impl->pipe.std_in.fd_write);
    }

    _started.store(success);
    return success;
}

DWORD Process::pid() const
{
    return _impl->process_watcher.procinfo.dwProcessId;
}

HANDLE Process::process() const
{
    return _impl->process_watcher.procinfo.hProcess;
}

bool Process::wait_process_end(Duration nano_duration)
{
    return _waitable.wait_for(nano_duration, [this] { return this->is_process_running() == false; });
}

bool Process::can_read_pipes() const
{
    return _impl->pipe.std_in.fd_read != nullptr || _impl->pipe.std_out.fd_read != nullptr
           || _impl->pipe.std_err.fd_read != nullptr;
}

bool Process::read_pipes(int milliseconds_timeout)
{
    if (this->can_read_pipes() == false)
        return false;
    SteadyClock clock;
    const Timestamp begin = clock.now();
    const Duration timeout = time::milliseconds(milliseconds_timeout);
    bool timed_out = false;
    while (!timed_out)
    {
        _impl->pipe.std_out.process_read_pipe();
        _impl->pipe.std_err.process_read_pipe();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        timed_out = (clock.now() - begin) >= timeout;
    }
    return true;
}

bool Process::terminate()
{
    this->read_pipes();

    safe_close(_impl->pipe.std_in.fd_write);
    safe_close(_impl->pipe.std_out.fd_read);
    safe_close(_impl->pipe.std_err.fd_read);

    _poll.stop();
    _poll.clear_fds();

    int tries = 3;
    while (this->is_process_running() && tries > 0)
    {
        this->wait_no_hang();
        if (this->is_process_running() == false)
            break;
        --tries;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (this->is_process_running())
    {
        this->kill();
        this->wait();
    }

    return this->is_process_running() == false;
}

bool Process::kill(int sig)
{
    if (sig < 0)
    {
        sig = 15;
    }
    bool ret = this->is_process_running();
    if (ret)
    {
        // own the process handle: kill it with a POSIX-like code so return_code() matches unix
        (void)sig;
        ret = TerminateProcess(_impl->process_watcher.procinfo.hProcess, failure_return_code) != 0;
        if (!ret)
            SIHD_LOG(error, "Process: could not kill: {}", os::last_error_str());
    }
    return ret;
}

// Run

void Process::handle(Poll *poll)
{
    (void)poll;
}

bool Process::on_start()
{
    const bool ret = this->execute();

    if (ret)
    {
        this->service_set_ready();
        // exits when the child terminates (reaped by wait_no_hang) or when on_stop's
        // reset_proc terminates the process, mirroring the unix poll loop in handle()
        while (this->is_running() && this->is_process_running())
        {
            constexpr DWORD timeout_ms = 50;
            this->read_pipes(timeout_ms);
            this->wait_no_hang();
        }
    }

    return ret;
}

bool Process::on_stop()
{
    this->reset_proc();
    return true;
}

// Check process

bool Process::is_process_running() const
{
    return _started.load();
}

bool Process::wait_no_hang()
{
    constexpr DWORD timeout_ms = 5;
    return this->wait(timeout_ms);
}

bool Process::wait(int options)
{
    if (this->is_process_running() == false)
        return false;
    _impl->process_watcher.check_status(options);
    const bool terminated = _impl->process_watcher.has_terminated();
    if (terminated)
    {
        auto l = _waitable.guard();
        _started.store(false);
        _waitable.notify_all();
    }
    return terminated;
}

Process::ReturnCodeType Process::return_code() const
{
    return _impl->process_watcher.return_code();
}

bool Process::has_terminated() const
{
    return _impl->process_watcher.has_terminated();
}

bool Process::wait_exit(int options)
{
    return this->wait(options);
}

bool Process::wait_any(int options)
{
    return this->wait(options);
}

bool Process::has_exited() const
{
    return _impl->process_watcher.has_terminated();
}

} // namespace sihd::sys
