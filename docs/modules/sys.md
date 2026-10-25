# sys

The `sys` module wraps operating-system facilities behind typed C++ APIs: files and filesystem queries, child processes, signals, inter-process primitives, desktop input and capture, users and privileges, and system information. Most APIs are portable across Linux and Windows; Linux-only and Windows-only features live in `sihd::sys::linux_` and `sihd::sys::windows_`. Most functions report errors through `sihd::util::Result`, and platform-dependent APIs expose a `supported` constant that tells whether they are built for the current platform.

| | |
|---|---|
| Depends on | `util` |
| External libraries | Linux: libuuid, libcap, libdbus-1. Windows: system libraries (advapi32, ole32, psapi, ws2_32, runtimeobject, and others). Optional desktop backends: X11, Xtst, Xrandr, wayland-client |
| Platforms | Linux, Windows. Android and Emscripten are partial: check the `supported` constant of platform-dependent APIs |
| Build | `make m=sys` |
| Header | `#include <sihd/sys/Process.hpp>` (one header per class or namespace) |

## Capabilities

### Files and filesystem

- `File`: buffered wrapper over a `FILE *` or a file descriptor, with `open`, `read`, `write`, `read_line`, `seek`, `tell`, `flush`, `set_buffer_size` and `lock`/`trylock`.
- `LineReader`: buffered line reader over a `File` or a path: `open`, `read_next`, `get_read_data`. `fast_read_line` reads one line from a `FILE *` (default `stdin`).
- `FileMutex`: inter-process lock on a file: `lock`, `try_lock`, `try_lock_for`, `lock_shared`, `unlock`.
- `MappedFile`: memory-mapped file: `create`, `open_read_only`, `open_read_write`, `data`, `size`, `sync`, `clear`.
- `TmpDir`: temporary directory removed on destruction; `path()`.
- `PathManager`: ordered search path list: `push_front`, `push_back`, `find`, `find_all`, `remove`.
- `fs` (namespace): path helpers (`combine`, `parent`, `filename`, `extension`, `normalize`, `realpath`, `jail`); queries (`exists`, `is_file`, `is_dir`, `is_symlink`, `file_size`, `times`, `free_space`, `total_space`, `mounts`, `mount_type`, `storage_medium`); permissions (`permission_get`, `permission_set`, `permission_add`, `permission_rm`, `permission_to_str`); operations (`make_directories`, `remove_directories`, `copy_file` with progress callback, `rename`, `truncate`, `make_file_link`, `make_dir_link`, `make_hard_link`, `read_link`, `chdir`); content (`read_all`, `read`, `read_line`, `read_lines`, `write` and `write_binary` with append).

### File watching

- `FileWatcher`: observable runnable reporting `FileWatcherEvent` values for watched paths: `watch`, `unwatch`, `is_watching`, `run`, `events`. The `opened`, `accessed` and `closed` event types are Unix-only.
- `FilePoller`: polling alternative that reports files created, removed or changed under a tree: `watch`, `unwatch`, `run`, `created`, `removed`, `changed`.
- `FileCache`: in-memory cache of file contents, kept coherent through a `FileWatcher`: `add`, `get`, `remove`, `clear`.

### Processes

- `Process`: spawns a child from an argument list or a function and manages it: `execute`, `wait_exit`, `wait_any`, `has_exited`, `return_code`, `kill`, `terminate`. Redirections: `stdin_from`, `stdout_to` and `stderr_to` (string, callback, fd or another `Process`), `*_close`, `stdout_to_file`, `stderr_to_file`. Environment: `env_set`, `env_get`, `env_rm`, `env_load`, `env_clear`. Execution settings: `set_chdir`, `set_nice`, `set_scheduler`, `set_affinity`. POSIX-only: `set_chroot`, `set_force_fork`, `wait_stop`, `wait_continue` and the signal-exit queries.
- `proc::execute`: runs a command asynchronously and returns `std::future<int>`. Configured by `proc::Options` (timeout, stdin, stdout and stderr callbacks, environment, nice, scheduler, affinity). Not available on Emscripten (`proc::supported`).
- `Environment`: environment variable set used by `Process`: `from_current`, `load`, `set`, `get`, `rm`, `clear`, `expand`.
- `program`: `path()` of the running executable and `find_symbol` in the loaded binaries.
- `ProcessInfo`: inspects one process by pid or name: `pids`, `get_all_process_from_name` (regex), `name`, `cmd_line`, `exe_path`, `cwd`, `env`, `creation_time`, `metrics`, `write_into_stdin`.
- `ProcessMonitor`: observable periodic sampler of matching processes: `set_frequency`, `set_name_filter`, `start`, `stop`, `samples`, `sample(pid)`.
- `sched` (namespace): per-process nice value, scheduling policy and priority, and CPU affinity: `nice`, `set_nice`, `scheduler`, `set_scheduler`, `scheduler_priority`, `affinity`, `set_affinity`.

### Applications and daemons

- `App`: CLI application built on `util::CliApp`. Maps signals to actions (`SigAction`: `stop`, `ignore`, `reload`, `callback`), exposes `daemon()` and `on_signal`.
- `Daemon`: runs the process as a daemon with user, group, pid file and working directory: `set_user`, `set_group`, `set_pid_file_path`, `set_working_dir_path`, `run`. Not available on Windows.
- `CrashHandler`: installs a handler for fatal signals that writes a backtrace to `Options::dump_path`: `install`, `uninstall`, `is_installed`.

### Signals

- `signal` (namespace): process-wide signal handling: `handle`, `unhandle`, `ignore`, `set_exit_config`, `status`, `last_received`, `stop_received`, `termination_received`, `dump_received`, `should_stop`, `reset_received`, `kill`, `name`. `block_thread` and `unblock_thread` return `not_supported` on Windows.
- `SigHandler`: RAII handler for one signal: `handle`, `unhandle`, `call_previous_handler`, `is_handling`.
- `SigWatcher`: observable watcher that runs a worker thread and calls a callback when watched signals arrive: `add_signal`, `rm_signal`, `set_polling_frequency`, `start`, `stop`, `is_running`. Uses `signalfd` on Linux and `sigwait` on other POSIX systems.
- `SigWaiter`: waits for one configured signal up to a timeout (`Conf`): `received_signal`.
- `SigThreadBlocker`: RAII guard that blocks signals on the calling thread (POSIX-only).

### Inter-process communication and I/O

- `SharedMemory`: named shared memory segment: `create`, `attach`, `attach_read_only`, `data`, `size`, `clear`.
- `Semaphore`: named cross-process semaphore: `create`, `attach`, `wait`, `try_wait`, `wait_for`, `post`, `value`. Not available on Android or Apple.
- `MessageQueue`: named queue of byte messages with priorities: `create`, `open`, `send`, `try_send`, `timed_send`, `receive`, `try_receive`, `timed_receive`, `set_nonblocking`. Not available on Android or Apple.
- `MemLock`: RAII page lock for a memory range (wraps `os::mem_lock`).
- `Poll`: observable blocking service that waits for file descriptor readiness: `set_read_fd`, `set_write_fd`, `rm_read_fd`, `rm_write_fd`, `set_timeout`, `poll`, `events`.
- `Pty`: pseudo-terminal abstraction; `create` returns the platform implementation: `set_shell`, `set_args`, `set_env`, `set_size`, `spawn`, `read`, `write`, `resize`, `send_eof`, `wait`, `terminate`. Check `Pty::is_supported()` first.
- `StreamCapture`: captures everything written to a `FILE *` (for example stdout) and returns it with `str()`.

### Dynamic loading

- `DynLib`: shared library handle: `open`, `open_system`, `load_symbol`, `load_function<F>`, `close`, `is_open`. `DynLib(file)` opens at construction and throws `std::runtime_error` on failure. `open` hands its file name or path to the loader untouched, no prefix or extension guessed. `open_system` refuses a path and loads a system library by name only: Windows searches System32 alone (`LOAD_LIBRARY_SEARCH_SYSTEM32`), so a planted DLL in the application directory, the current directory or the `PATH` is never picked; Linux goes through the dynamic loader search path, which never includes the current directory. `load_function<F>` returns the symbol as an `F *` function pointer. `DynLib::supported` is false where the libc has no usable loader: Emscripten and statically linked Linux builds (musl's static `dlopen` always fails, static glibc loads a second libc). Static Windows builds keep it: `LoadLibrary` lives in kernel32, always dynamic.
- `PluginLoader`: creates `Named` objects from a library (`create_from_library`) or from a class linked in the binary (`create_from_linked`), and unloads libraries (`unload`). `create_from_library` takes the library name and loads `lib<name>.so` on Linux, `<name>.dll` on Windows.

### Desktop integration

Linux backends (X11, Wayland) compile only when the `x11=1` or `wayland=1` build option is set. Windows backends are always built. Check each namespace's `supported()` function: it is a runtime call because the backends are a build option, so no define leaks into the public headers.

- `clipboard`: `get_raw` (all or selected MIME types), `to_text`, `to_image`, `set` (text, `Bitmap` or raw MIME data), `set_raw`.
- `screenshot`: `take_screen`, `take_under_cursor`, `take_focused`, `take_window_name`, each filling a `Bitmap`.
- `screen`: `monitors`, `primary`, `containing(x, y)`, returning `screen::Monitor` geometry, scale and DPI.
- `keyboard`: `type_text` (UTF-8), `key_press` with `Modifiers`, `key_down`, `key_up`.
- `mouse`: `mouse_move`, `mouse_move_by`, `mouse_button`, `mouse_click`, `mouse_scroll`.
- `Bitmap`: in-memory pixel buffer: `create`, `fill`, `get`, `set`, `save_bmp`, `read_bmp`, `to_bmp_data`, `read_bmp_data`.
- `DesktopNotifier`: desktop notifications: `notify` a `Notification` (summary, body, icon, `Urgency`, optional timeout) and get its id, `close(id)`. A negative timeout is rejected (`invalid_argument`). Linux sends them to the `org.freedesktop.Notifications` service of the session bus (D-Bus): the icon is an icon name or a path, the urgency maps to the `urgency` hint, the timeout to the expiration (server default when unset); the server capabilities are asked once per connection so that the body is escaped when the server reads markup; `set_app_name` and `set_app_id` (the `desktop-entry` hint) are optional; ids come from the server, so any notifier can `close` them. Windows shows WinRT toasts and requires `set_app_id` (AppUserModelID): the first `notify` registers it under `HKCU\Software\Classes\AppUserModelId\<id>` (per user, no admin rights, how unpackaged apps get a notification identity) with `set_app_name` as display name; the icon is an image path, `low` shows a silent toast, `critical` a long one (`duration="long"`), `normal` a short one with sound; the timeout is ignored (the system sets the display time); ids are per notifier and `close` only reaches the toasts of its own notifier. Other platforms return `not_supported`.

### Users, privileges and sessions

- `user` (namespace): `UserId` and `GroupId` (`BasicId`), `effective_user`, `real_user`, `effective_group`, `groups`, `name`, `name_of`, `user_id_of`, `group_id_of`, `primary_group_of`, `is_root`. `drop_privileges` is POSIX-only.
- `Impersonation`: runs code as another user: `impersonate_with_credentials` (Windows), `impersonate_as` (Linux, privileged), `revert`, `impersonating`.
- `cap` (namespace) and `CapabilitySet`: capability names and state (`all`, `from_name`, `to_name`, `available`, `has`, `set_ambient`, `drop_bounding`), and per-process raise, drop and apply through `CapabilitySet`. Linux exposes the full capability list (`cap::count` is 41); other platforms expose the common subset.
- `session::sessions`: logged-in sessions with user, device, host and state.
- `power`: `batteries` and `ac_lines` with charge, state and time estimates.

### System information

- `info` (namespace): `total_ram`, `available_ram`, `total_swap`, `free_swap`, `cpu_count`, `physical_core_count`, `cpu_times`, `CpuUsage` (`sample`, then `usage` and `per_core_usage`), `cpu_frequency`, `os_info`, `hostname`, `domain`, `uptime`, `load_average`, `hardware_identity`.
- `os` (namespace): file descriptor helpers (`fd_type`, `is_tty`, `is_pipe`, `set_nonblocking`, `bytes_available`, `dup`, `dup2`, `ioctl`, `setsockopt`), error strings (`last_error_str`, `last_socket_error_str`), memory (`current_rss`, `peak_rss`, `mem_lock`, `mem_unlock`), `backtrace` (real output on glibc and Windows only), `exists_in_path`, and detection of valgrind, qemu, debugger and Wine.
- `env` (namespace): `get`, `set`, `unset`, `list`, `expand` on the current process environment.
- `console` (namespace): `size`, `is_interactive`, `enable_vt`, `set_output_utf8`; RAII guards `RawMode` and `NoEcho` on a file descriptor.
- `services`: `service_port` and `service_name` lookups from the system service database.
- `Uuid`: UUID parsing and name-based UUIDs under a namespace (`Uuid(namespace, name)`); namespace constants `DNS()`, `URL()`, `OID()`, `X500()`.

### Logging

- `LoggerFile`: `ALogger` sink that writes to a file: `set_path`, `set_append`, `set_pattern`, `is_open`.
- `LoggerSystem`: `ALogger` sink for the system log: syslog on Linux, the Windows Event Log on Windows. `set_facility` applies to syslog.

### Linux only (`sihd/sys/linux/`, namespace `sihd::sys::linux_`)

Every header in `sihd/sys/linux/` raises a compile error on other platforms.

- `linux_::dbus::Connection`: D-Bus connection to the `session` or `system` bus, or to a bus address: `open`, `is_open`, `add_match`, `remove_match`, `request_name`, `name_has_owner`, `interrupt`, and `disable_auto_start` so its calls fail instead of starting an unowned activatable service. `Message` builds method calls, signals and replies (`method_call`, `signal`, `method_return`, `error_return`, `properties_get`, `properties_get_all`, `add`, `add_object_path`) and reads them (`type`, `path`, `interface`, `member`, `sender`, `error_name`, `to_error`, `changed_property` of a `PropertiesChanged` signal). `call` waits for one reply, `call_all` sends every request before awaiting the replies, in request order; `send`, `receive` and `receive_until` (the first accepted message, `timeout` otherwise) stay asynchronous. An error reply becomes `permission_denied` for `AccessDenied`, `timeout` for `NoReply`, `io_error` otherwise, its message starting with the error name (`has_error_name`). `Appender` writes values and containers (`add_array`, `add_entry` for an `a{sv}` entry, `open_array`, `open_variant`, `open_struct`, `open_dict_entry`, `close`) checked against their signature and their nesting order, so libdbus never aborts; `Iter` reads them back (`get`, `get_array`, `for_each_entry` with the variants unwrapped). `get_property` / `get_array_property` read one property, `signal_rule` / `properties_changed_rule` build match rules. Not available on Android or Emscripten.
- `linux_::prctl`: process control flags: `set_pdeathsig`, `set_dumpable`, `set_no_new_privs`, `set_child_subreaper`, with matching getters.
- `linux_::cgroup`: cgroup v2 state and limits: `is_container`, `current_group`, `memory_max`, `memory_current`, `cpu_max`, `write_memory_max`, `write_cpu_max`.
- `linux_::ns`: Linux namespaces: `unshare`, `setns`, `id`, `to_str`.
- `linux_::limit`: resource limits: `get` and `set` for a `Resource` (rlimit).
- `linux_::EventFd`: eventfd counter: `open`, `notify`, `read`, `fd`.
- `linux_::TimerFd`: timerfd: `open`, `set(initial, period)`, `disarm`, `wait`, `fd`.

### Windows only (`sihd/sys/windows/`, namespace `sihd::sys::windows_`)

Every header in `sihd/sys/windows/` raises a compile error on other platforms.

- `windows_::registry`: `Key` opens or creates a key under a `Hive` and reads and writes typed values (`string`, `strings`, `u32`, `u64`, `binary` and the matching `set_*`), with `subkeys`, `value_names`, `remove_value`. `registry::remove` deletes a key.
- `windows_::scm`: service control: `Service` with `open`, `create`, `start`, `stop`, `status`, `set_start_type`, `remove`.
- `windows_::Job`: job object: `create`, `set_limits` (process and job memory, per-process CPU time, kill on close), `assign`, `terminate`, `accounting`, `wait_event`.
- `windows_::ComInit`: RAII COM initialization, apartment or multithreaded.
- `windows_::uac::relaunch_elevated`: restarts the executable elevated with the given arguments.
- `windows_::volume`: `query` (root, label, file system, media type, serial) and `eject`.

## Example

```cpp
#include <string>
#include <vector>

#include <sihd/sys/proc.hpp>

using namespace sihd::sys;

int main()
{
    std::string output;
    std::vector<std::string> cmd = {"echo", "test_output"};

    auto future = proc::execute(cmd,
                                proc::Options {.stdout_callback = [&output](std::string_view sv) { output += sv; }});

    int ret = future.get();
    // ret == 0 on success; output holds the captured stdout
    return ret;
}
```

Adapted from `modules/sys/test/TestProc.cpp`.

## Scripting

Exposed to Lua (`LuaSysApi`) and Python bindings.
