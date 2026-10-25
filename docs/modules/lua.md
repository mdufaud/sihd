# lua

Lua 5.3 bindings for sihd. `sihd::lua::Vm` embeds a Lua state in a C++ program, and per-module binding files register sihd classes and functions in that state under the global `sihd` table. Each binding is compiled only when its sihd module is part of the build, so the set of bindings depends on the build.

| | |
|---|---|
| Depends on | `util`, plus any of `sys`, `core`, `json`, `csv`, `zip`, `net`, `http` that are in the build |
| External libraries | lua 5.3, luabridge3 |
| Platforms | all (the `net` and `http` bindings are not built for web) |
| Build | `make lua=1 m=util,sys,core,lua` (`lua=1` is required; list the sihd modules whose bindings you want next to `lua`, a lone `m=lua` builds no bindings) |
| Header | `#include <sihd/lua.hpp>` (umbrella), or `#include <sihd/lua/Vm.hpp>` |

## Capabilities

The header `sihd/lua/config.hpp` is generated in the build directory and defines `SIHD_LUA_WITH_<MODULE>` for each binding compiled. `sihd/lua.hpp` includes only the binding headers whose define is set.

### Embedding

- `Vm`: owns a `lua_State` created by its default constructor, or borrows one through `Vm(lua_State *)` or `set_state()`.
- `Vm::new_state()`, `Vm::close_state()`, `Vm::set_state(lua_State *)`: replace or release the state.
- `Vm::do_file(path)` and `Vm::do_string(str)`: load and run a chunk, return `sihd::util::Result<void>`.
- `Vm::set_ref<T>(name, obj)`, `Vm::get_ref(name)`, `Vm::new_table()`, `Vm::ref_exists(name)`, `Vm::refs_exists({...})`: read and write Lua globals from C++.
- `Vm::new_luathread()`, `Vm::new_thread(Vm &)`: create a coroutine state that shares the universe of the parent.
- `Vm::dump_stack(max)`, `Vm::print_stack(max, FILE *)`: stack inspection for debugging.
- `Vm::get_vm(lua_State *)`, `Vm::get_gil(lua_State *)`: recover the `Vm` or the lock of a state.
- `LuaGil`: per-universe lock, reentrant per thread. `LuaGilGuard` holds it for a region; `LuaGilRelease` releases it around a blocking C++ call. `Vm` methods take the guard themselves.

### Loading bindings

- `LuaUtilApi::load_base`, `load_tools`, `load_threading`, `load_all`: util bindings. `load_all` calls the three others in order.
- `LuaSysApi::load_base`, `load_tools`, `load_files`, `load_process`, `load_all`: sys bindings. `load_base` also sets `sihd.dir` to the executable directory.
- `LuaCoreApi::load(vm)` and `LuaCoreApi::unload(vm)`: core bindings.
- `LuaNetApi::load_base`, `load_all`; `LuaHttpApi::load_base`, `load_all`; `LuaJsonApi::load_base`, `load_all`; `LuaCsvApi::load_base`, `load_all`; `LuaZipApi::load_base`, `load_all`.
- Load the util bindings before the other ones, as `luabin` and the core tests do.

### Reachable from scripts (`sihd.<module>`)

- `sihd.Error`: raised by a failing binding, caught with `pcall`; `code` compares to `sihd.ErrorCode.<name>` (the `sihd::util::ErrorCode` names), `message`, `retryable()`.
- `util` (`LuaUtilApi`): `Node`, `Named`, `AService`, `ServiceController`, `IArray` and typed arrays (`ArrBool` to `ArrDouble`), `Splitter`, `Timestamp`, `Duration`, `Random`, `IClock`, `SystemClock`, `SteadyClock`, `Waitable`, `Scheduler` (`add_task`, `start`, `stop`), `Worker`, `StepWorker`; namespaces `time`, `thread`, `str`, `num`, `endian`, `log` (`info`, `error`, ... `console`, `stream`, `clear`).
- `sys` (`LuaSysApi`): `Uuid`, `ProcessInfo`, `Color`, `Bitmap`, `FileMutex`, `Process`, `ProcFuture`, `File`, `LineReader`; namespaces `sihd.sys.fs`, `sihd.sys.os`, `sihd.sys.user` (`name` returns `nil` on failure), `sihd.sys.signal`, `sihd.sys.screenshot`.
- `core` (`LuaCoreApi`): `Device`, `Core`, `Channel` (`read`, `write`, `write_array`, `set_observer`), `ACoreObject`, `ChannelWaiter`, `DevFilter`, `DevPulsation`, `DevSampler`, `DevPlayer`, `DevRecorder`, `DevMessage`.
- `net` (`LuaNetApi`): `IpAddr`, `sihd.net.ip_addr(host[, port])`, `DeviceTcpClient`, `DeviceTcpServer`, `DeviceUdpSender`, `DeviceUdpReceiver`, `NetInterface` (`get_all_interfaces`, kinds in `NetInterface.Kind`), `route.find` / `route.defaults_ipv4` / `route.defaults_ipv6`, `dns.servers`, `NetMonitor.open([timeout])` (`run` then `events`). Failures raise a bound `Error`.
- `wifi` (`LuaWifiApi`): enums `Security`, `Backend`, `State`, `Link`, `AccessPoint`, `KnownNetwork`, `link`, `access_points`, `security_of` (a string of elements), `scan`, `backend`, `state`, `connect(interface, ssid[, passphrase], timeout)`, `disconnect`, `known_networks`, `forget`, `Monitor.open(interface[, timeout])` (`run` then `events` of `Monitor.Event`, `Monitor.Event.Type`), and the passive 802.11 decoders (`dissect_radiotap`, `dissect_80211`, `Packet`, `Radiotap`, `Frame`, `Addresses`, the `parse_*` management and LLC parsers, the `ies` namespace). Timeouts are a `Duration` or nanoseconds. Failures raise a bound `Error`.
- `http` (`LuaHttpApi`): `Navigator` (`get`, `post`, `put`, `del`, `download`, ...), `RequestOptions`, `Multipart`, `HttpRequest`, `HttpResponse`, `WebService`, `HttpServer`.
- `json` (`LuaJsonApi`): `sihd.json.decode` / `parse`, `sihd.json.encode` / `dump`.
- `csv` (`LuaCsvApi`): `CsvReader`, `CsvWriter`.
- `zip` (`LuaZipApi`): `ZipFile`.

## Example

```cpp
#include <sihd/lua/Vm.hpp>
#include <sihd/lua/core/LuaCoreApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>

void run_channel_script()
{
    sihd::lua::Vm vm;

    sihd::lua::LuaUtilApi::load_base(vm);
    sihd::lua::LuaUtilApi::load_tools(vm);
    sihd::lua::LuaCoreApi::load(vm);

    auto res = vm.do_file("test/core/lua/test_channel.lua");
    if (!res)
        return;
}
```

Adapted from `modules/lua/test/core/TestLuaCoreApi.cpp` (`test_luacoreapi_channel`).

Script side, the body of `modules/lua/test/core/lua/test_channel.lua` (first lines):

```lua
local core = sihd.core.Core("core", nil)
local ch_bool = sihd.core.Channel("ch_bool", "bool", 1, core)

assert(ch_bool:name() == "ch_bool")
assert(ch_bool:write(0, true))
assert(ch_bool:read(0) == true)
```

Adapted from `modules/lua/test/core/lua/test_channel.lua`.

## Scripting

Exposed to Lua through the `sihd` global table (this module is the Lua binding layer). Not exposed to Python. The `sihd_lua` REPL is described in `docs/modules/luabin.md`.
