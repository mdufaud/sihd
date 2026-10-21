# SIHD

Simple Input Handler Displayer

Aimed to be a simple C++ library with some python and/or LUA bindings that gets, parse and displays data.

## Modules

| Module | Description | Dependencies |
|--------|-------------|--------------|
| `json` | JSON parsing/serialization | simdjson |
| `util` | Base: logging, services, threads, scheduling, observers, CLI, time, containers, named nodes | json, fmt, cli11 |
| `sys` | OS abstraction: files, processes, IPC, signals, dynamic loading, clipboard, screenshot, input injection, users, Linux/Windows-only APIs | util |
| `core` | Devices, Channels, recording/playback, sampling, profiling | util, sys |
| `crypto` | Keys, certificates, TLS context, ciphers, digests, signatures | util, sys, openssl |
| `curl` | libcurl transfer wrapper | util, curl |
| `net` | Sockets, TCP/UDP/ICMP, TLS, DNS, interfaces | sys, core, crypto |
| `http` | HTTP/WebSocket server, HTTP client | curl, net, libwebsockets, libuv, zlib, openssl |
| `pcap` | Packet capture and capture files | net, libpcap |
| `csv` | CSV reading/writing | util, sys |
| `zip` | Archive compression and encryption | util, sys, libzip |
| `tui` | Terminal UI components (ftxui) | util, sys, ftxui |
| `ssh` | SSH client and server, SFTP | util, sys, libssh |
| `usb` | USB device access (placeholder) | util, sys, libusb |
| `bt` | Bluetooth LE scanning | util, sys |
| `imgui` | Dear ImGui runner (GLFW, SDL3, Win32, Android, ncurses / OpenGL3, DirectX11, ncurses) | util, sys, imgui, opengl |
| `lua` | Lua bindings (conditional: `lua=1`) | util, lua 5.3, luabridge3 |
| `luabin` | Standalone Lua interpreter binary (conditional: `lua=1`) | lua |
| `py` | Python bindings via pybind11 (conditional: `py=1`) | pybind11, python3 |

### Module reference

#### `json`

JSON document parsing, typed access and serialization on top of simdjson.

- **Document**: `Json` (`parse`, `dump`, `get`, `get_or`, `contains`, `type`, `is_object`, `is_array`, iteration)
- **Helpers**: `utils::find_first`, `utils::for_each`

#### `util`

Base layer used by every other module.

- **Logging**: `Logger`, `LoggerManager`, `LoggerAsync`, `LoggerConsole`, `LoggerStream`, `LoggerThrow`, `LoggerFilter`, `LogFormatter`, `LogInfo`
- **Services**: `AService`, `ServiceController`, `ThreadedServiceController`, `BlockingServiceController`, `AThreadedService`, `ABlockingService`, `AWorkerService`, `AStepWorkerService`
- **Threads and scheduling**: `ThreadPool`, `Worker`, `Runnable`, `Task`, `Scheduler`, `Timer`, `TimedHandler`, `RateLimiter`, `TokenBucket`, `Waitable`, `Synchronizer`, `SafeQueue`
- **Observers and callbacks**: `Observable`, `ObserverWatcher`, `ObserverWaiter`, `Callback`, `CallbackManager`, `Handler`, `IHandler`, `MoveOnlyFunction`, `Collector`
- **Command line**: `CliApp`, `CliInterpreter`, `Command`, `TokenPattern`, terminal helpers in `ansi.hpp`
- **Time**: `Timestamp`, `Duration`, `Clock`, `SteadyClock`, `SystemClock`, `Stopwatch`, `TimeBase`, conversions in `time.hpp`
- **Containers**: `Array`, `ArrayView`, `IArray`, `RingBuffer`, `Slice`, `Cache`, `Splitter`, helpers in `container.hpp` and `array_utils.hpp`
- **Named tree and values**: `Named`, `Node`, `NamedFactory`, `Value`, `Message`, `DynMessage`, `MessageField`, `Provider`, `FunctionProvider`
- **Configuration**: `Configurable`, `StrConfiguration`
- **Strings**: `str.hpp` (`split`, `join`, `trim`, `regex_match`, `glob_match`, `format_time`), `Url`, `LoadingBar`, `mime.hpp`, `locale.hpp`
- **Errors**: `Error`, `ErrorCode`, `Result` (`std::expected<T, Error>`)
- **Statistics and profiling**: `Stat`, `PSquareStat`, `Timeit`, `Perf`
- **Misc**: `Random`, `StateMachine`, `Defer`, `ScopedModifier`, `endian.hpp`, `hash.hpp`
- **Platform**: `build.hpp` (`__SIHD_LINUX__`, `__SIHD_WINDOWS__`, `__SIHD_EMSCRIPTEN__`)

#### `sys`

OS facilities, with Linux-only and Windows-only namespaces.

- **Files**: `File`, `FileMutex`, `MappedFile`, `LineReader`, `TmpDir`, `PathManager`, `fs` (paths, mounts, storage medium)
- **File watching**: `FileWatcher`, `FilePoller`, `FileCache`
- **Processes**: `Process` (spawn from args or function), `proc::execute`, `ProcessInfo`, `ProcessMonitor`, `sched` (nice, policy, affinity), `program` (own path, symbol lookup)
- **Applications**: `App` (CLI app with signal actions), `Daemon` (user, group, pid file, working directory), `CrashHandler`
- **IPC and I/O**: `SharedMemory`, `Semaphore`, `MessageQueue`, `MemLock`, `Poll`, `Pty`, `StreamCapture`
- **Signals**: `signal` (handle, unhandle, ignore), `SigHandler`, `SigWatcher`, `SigWaiter`, `SigThreadBlocker`
- **Dynamic loading**: `DynLib`, `PluginLoader`
- **Desktop**: `clipboard` (text, bitmap, raw MIME), `screenshot` (screen, under cursor, focused, window by name), `screen` (`Monitor`), `Bitmap`
- **Input injection**: `keyboard` (type text, key press/down/up), `mouse` (move, buttons, scroll); X11, Wayland or Windows
- **Users and privileges**: `user`, `Impersonation`, `CapabilitySet`, `cap`, `session`, `power` (battery, AC line)
- **System info**: `info` (`CpuUsage`, `OsInfo`, hostname), `env`, `Environment`, `services`, `Uuid`, `console` (raw mode, no echo, window size), `os`
- **Logging backends**: `LoggerSystem` (syslog), `LoggerFile`
- **Linux only** (`sys/linux/`): `linux_::dbus`, `linux_::prctl`, `linux_::cgroup`, `linux_::ns`, `linux_::limit`, `linux_::EventFd`, `linux_::TimerFd`
- **Windows only** (`sys/windows/`): `windows_::registry`, `windows_::scm`, `windows_::Job`, `windows_::ComInit`, `windows_::uac`, `windows_::volume`

#### `core`

Device/Channel layer and ready-made devices.

- **Base classes**: `Device` (service lifecycle with `on_*` hooks), `Channel` (typed array bus with observers), `AChannelContainer`, `ACoreObject`, `ACoreService`
- **Root container**: `Core` (cascades service state to its children)
- **Recording and playback**: `DevRecorder`, `MemRecorder`, `DevPlayer`, `Records.hpp` (`PlayableRecord`, `SortedRecordedValues`)
- **Timing**: `DevPulsation` (heartbeat counter channel), `DevSampler` (samples channels at a frequency)
- **Transformation**: `DevFilter` (rule-based channel-to-channel writes, optional delay), `DevMessage` (maps channels to `IMessageField` paths)
- **Helpers**: `ChannelMatch` (typed value comparison), `ChannelWaiter`
- **Profiling**: `TreeProfiler` (traces channel notifications and service operations)

#### `crypto`

OpenSSL keys, certificates, TLS context, ciphers, digests and signatures.

- **Keys**: `PrivateKey` (RSA/EC generation, PEM load/save)
- **Certificates**: `Certificate` (self-signed generation, PEM/DER load, verification, CN, SAN, validity, CA flag)
- **TLS**: `TlsContext` (client/server mode, certificate and key, CA loading, peer verification)
- **Encryption and hashing**: `cipher::encrypt`, `cipher::decrypt`, `digest::compute`
- **Signatures**: `utils::sign_data`, `utils::verify_data`

#### `curl`

libcurl transfer wrapper.

- **Transfers**: `Request`, `XferProgress`, `Auth`, `Proxy`
- **Headers and MIME**: `HeaderList`, `Mime`
- **Helpers**: `resolve_url`, `version`

#### `net`

Socket networking (IP and Unix) with TLS, DNS, interfaces and `core` device wrappers.

- **Addresses**: `IpAddr`, `Subnet`, `ip::` helpers (`is_valid_ip`, `is_private_host`, `public_addresses`)
- **Sockets**: `Socket` (IPv4/IPv6/Unix)
- **TCP**: `TcpClient`, `TcpServer`, `BasicServerHandler` (client limit, idle timeout)
- **UDP**: `UdpSender`, `UdpReceiver`
- **ICMP**: `IcmpSender`, `Pinger`
- **TLS**: `TlsConnection`, `TlsSocket` (`tls_accept`, SNI)
- **DNS**: `dns::find`, `dns::lookup`, `dns::lookup_async`
- **Interfaces**: `NetInterface` (`get_all_interfaces`, flags, addresses, MAC)
- **Abstractions**: `INetServer`, `INetServerHandler`, `INetReceiver`, `INetSender`
- **Devices**: `DeviceTcpClient`, `DeviceTcpServer`, `DeviceUdpSender`, `DeviceUdpReceiver`

#### `http`

HTTP/WebSocket server on libwebsockets and HTTP client on `curl`.

- **Server**: `HttpServer` (port, root directory, SSL certificate/key, CORS, WebSocket endpoints)
- **Routing**: `Route`, `RouteTable`, `WebService`
- **WebSocket**: `IWebsocketHandler`, `WebsocketHandler`
- **Messages**: `HttpRequest` (path/query params, cookies, multipart, JSON), `HttpResponse` (text, bytes, JSON, file, streaming), `BodyStreamFile`, `BodyStreamFunc`
- **Headers and MIME**: `HttpHeader`, `HttpStatus`, `MimeTypes`, `Multipart`
- **Filters and auth**: `IHttpFilter`, `RateLimitFilter`, `IHttpAuthenticator`
- **Client functions**: `get`, `post`, `patch`, `del`, `head`, `options` and `async_*` variants with `RequestOptions`
- **Navigator**: `Navigator` (cookies, proxies, retries, basic/digest/token auth, form login, `download`, `put_file`), `RedirectPolicy`, `RobotsTxt`

#### `pcap`

libpcap capture and capture-file I/O.

- **Live capture**: `Sniffer` (BPF filter, promiscuous/monitor/immediate mode, stats)
- **Capture files**: `PcapReader`, `PcapWriter`
- **Devices**: `PcapInterfaces`, `PcapIFace`, `utils::lookupnet`

#### `csv`

Delimited file reading and writing with comment lines and timestamp columns.

- **Reading**: `CsvReader` (delimiter, comments, timestamp column and format)
- **Writing**: `CsvWriter` (`write_row`, `new_row`, `write_commentary`)
- **Helpers**: `utils::csv_from_string`, `utils::csv_to_str`, `utils::write_csv`, `utils::escape_str`

#### `zip`

Archive creation, extraction and editing with libzip.

- **One-shot**: `zip`, `unzip`, `list_entries`
- **Archive handle**: `ZipFile` (read entries, add files/directories, extract, rename, remove)
- **Encryption**: `ZipFile::set_password`, `encrypt_all`, `encrypt_in_aes_128`, `encrypt_in_aes_256`

#### `tui`

ftxui terminal components.

- **Command line**: `CliComponent` (`CliOptions`, `Cli`, `OutputStyle`)
- **Log viewer**: `LoggerComponent` (`LoggerOptions`)

#### `ssh`

libssh client and server.

- **Session**: `SshSession` (connect, host key check, auth methods), `SshKey` (generate, import, export)
- **Channels**: `SshChannel` (PTY, exec, TCP/Unix port forwarding, X11), `SshCommand`, `SshShell`
- **SFTP**: `Sftp` (list, get, send, mkdir, rename, chmod, rm)
- **Server**: `SshServer`, `ISshServerHandler`, `BasicSshServerHandler` (allowed users and public keys)
- **Server subsystems**: `ISshSubsystemHandler`, `SshSubsystemExec`, `SshSubsystemPty`, `SshSubsystemSftp`, `SshSubsystemFunction`

#### `usb`

Placeholder: `UsbUtils` is empty; libusb is only used by tests.

#### `bt`

Bluetooth LE scanning: BlueZ over D-Bus on Linux, WinRT advertisement watcher on Windows.

- **Scanning**: `Scanner` (`set_adapter`, `scan_for`, `results`, observable per device), `ScanResult`
- **Adapters**: `list_adapters`, `bluetooth_enabled`, `AdapterInfo` (Linux only)

#### `imgui`

Dear ImGui loop: `ImguiRunner` drives a platform backend and a renderer.

- **Backends**: `IImguiBackend`, `ImguiBackendGlfw`, `ImguiBackendSDL` (SDL3), `ImguiBackendWin32`, `ImguiBackendAndroid` (EGL/GLES3), `ImguiBackendNcurses` (Linux terminal)
- **Renderers**: `IImguiRenderer`, `ImguiRendererOpenGL`, `ImguiRendererDirectX` (DirectX11), `ImguiRendererNcurses`

#### `lua`

Lua 5.3 bindings (LuaBridge3) exposing compiled modules under the global `sihd` table.

- **Embedding**: `Vm` (`do_string`, `do_file`, `new_state`), `LuaGil`, `LuaGilGuard`
- **Exposed modules**: `LuaUtilApi` (with `LuaThreadRunner`, `LuaCoroutine`, `LuaScheduler`, `LuaWorker`), `LuaSysApi`, `LuaCoreApi`, `LuaNetApi`, `LuaHttpApi`, `LuaJsonApi`, `LuaCsvApi`, `LuaZipApi`
- **Build gating**: one `SIHD_LUA_WITH_<MODULE>` define per module present in the build

#### `luabin`

`sihd_lua` REPL: reads stdin lines and runs them in a `Vm` with every compiled Lua API loaded.

#### `py`

pybind11 extension module `sihd` with submodules `sihd.util`, `sihd.sys`, `sihd.core`, `sihd.net`, `sihd.http`, registered through `PyApi::add_api` for each module present in the build.

---

## Dependencies

### C++ 20

Used by core application.

### SCons 4.10

Manages compilation rules with python.

```shell
# recommended
apt install python3-pip
pip install scons
# or
apt install scons
```

If scons is installed from python-pip, don't forget to add to ~/.bashrc:

```shell
export PATH=$PATH:$HOME/.local/bin
```

To find binary from python-pip binaries folder.

### VCPKG (external libraries)

All dependencies are listed in [app.py file](app.py), only few are needed for each modules

#### Install globally on your system

This step is not needed if dependencies are directly installed on the building system and you don't need cross compiling.

```shell
# prints system external libraries list for a specific package manager
scons pkgdep=[apt|pacman] test=[0|1] modules=COMMA_SEPARATED_MODULES
```

#### Compile locally for architecture/cross building

Needed for Windows cross building or Emscripten building to get static libraries or if you don't want to install libraries on your system.



```shell
# needed by vcpkg
apt install curl zip unzip tar cmake ninja-build
```

Installs vcpkg inside the repository (no leaking in your system)

```shell
# to get all dependencies for every modules
make dep
# to be able to run unit tests
make dep test=1
# to get dependencies for single/multiple module.s
make dep mod COMMA_SEPARATED_MODULES
# to get windows dll dependencies
make dep platform=win
# for lua/python bindings
make dep lua=1 python=1
# dump installed library licenses (stdout + build/.../vcpkg/licenses.txt)
make dep licenses
```

#### Using MSYS2

If you want to compile for windows using MSYS2

```shell
pacman -S --needed git mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc
```

### Compilers

**g++ 15**

```shell
apt install g++
```

**clang++ 21**

```shell
apt install clang libc++-dev libc++abi-dev llvm
```

**g++-mingw 15-posix**

```shell
apt install g++-mingw-w64-x86-64
# configure mingw32 to posix
update-alternatives --config x86_64-w64-mingw32-g++
```

**emscripten 5**

```shell
apt install emscripten
```

```shell
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
# add to bashrc
source ./emsdk_env.sh
```

---

## Build

To build binaries and compile shared libraries with scons

```shell
make
```

To compile specific module.s

```shell
make mod COMMA_SEPARATED_MODULES
```

Flavours (you can mix them mostly)

```shell
# static libraries
make static=1
# address sanitizer
make asan=1
# no debug symbols
make mode=release
# build with clang
make compiler=clang
# build with emscripten (automatically builds with static libraries)
make compiler=em
```

### Cross compilation

```shell
# build for windows
make platform=win
# build for aarch64
make machine=aarch64
# build for riscv64
make machine=riscv64
# build for arm32
make machine=arm32 m=util,core
# build with musl libc
make libc=musl
```

### Musl

Download - extract - link in PATH: https://musl.cc/x86_64-linux-musl-cross.tgz 

### Python bindings build

**python 3**

It is recommended to install it on your system with a package manager: `python3-dev`

Add to your command line "py=1"

```shell
make py=1
# or
make mod core,py
```

/!\ not available with mingw windows cross compilation /!\

### Lua bindings build

**lua 5.3**

It is recommended to install it on your system with a package manager: `lua5.3-dev`

Add to your command line "lua=1"

```shell
make lua=1
# or
make mod core,lua
```

---

## Demo

Create demo for each for the compiled module

```shell
make dep mod COMMA_SEPARATED_MODULES demo=1
make demo COMMA_SEPARATED_MODULES
```

---

## Tests

To compile and run tests

```shell
# Run all tests
make test
# Run some modules tests
make test COMMA_SEPARATED_MODULES
```

List tests

```shell
# For all modules
make test ls
# For some modules
make test COMMA_SEPARATED_MODULES ls
```

Launch only specific tests

```shell
make test COMMA_SEPARATED_MODULES FILTER
```

Repeat tests

```shell
make test repeat=NUMBER
```

Run each module's tests as concurrent shards (faster on big modules; the `http` tests shift their loopback ports by the shard index)

```shell
make test jtest=4
```

Execute non interactive tests

```shell
make itest
```

Debug tests

```shell
# gdb
make gdbtest
# valgrind
make valtest
# compiled with libasan
make asantest
```

---

## Distribution

To create pacman distribution PKGBUILD file

```shell
make dist pacman
# for a single/multiple module.s
make dist mod COMMA_SEPARATED_MODULES pacman
```

To create apt distribution .deb file

```shell
make dist apt
# for a single/multiple module.s
make dist mod COMMA_SEPARATED_MODULES apt
```

To create a tar.gz with current build

```shell
make dist tar
# for a single/multiple module.s
make dist mod COMMA_SEPARATED_MODULES tar
```

---

## Installation

To install build into your system **/usr/local**

```shell
make install
# to change default install directory from /usr/local/lib -> pkg/usr/local/lib ...
make install INSTALL_DESTDIR=pkg
# to change default /usr/local to /usr
make install INSTALL_PREFIX=/usr
```

This will create into Makefile's folder a '.installed' containing every file installed

This file is used by 'uninstall' makefile rule to remove what has been installed.

```shell
make uninstall
```
