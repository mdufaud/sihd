# py

Python bindings for sihd, built on pybind11. The build produces the extension module `sihd`, whose submodules `sihd.util`, `sihd.sys`, `sihd.core`, `sihd.net` and `sihd.http` expose the sihd classes and functions of the modules compiled in. `PyApi` lets each binding register itself into the module before the interpreter imports it.

| | |
|---|---|
| Depends on | any of `util`, `sys`, `core`, `net`, `http` that are in the build |
| External libraries | pybind11, python3 (3.12) |
| Platforms | Linux |
| Build | `make py=1 m=util,sys,core,net,http,py` (`py=1` is required; a lone `m=py` builds no bindings) |
| Header | `#include <sihd/py/PyApi.hpp>` |

## Capabilities

### Embedding and registration

- `PyApi::add_api(std::string_view name, std::function<void(PyApi::PyModule &)>)`: register a submodule loader. The bindings call it from a static constructor, so the loader runs before `main()`.
- `PyApi::set_api_to_module(pybind11::module &)`: called by the generated `PYBIND11_MODULE(sihd)` entry point; loads every registered loader and sets `sihd.Error`.
- `PyApi::PyModule`: wraps the `sihd` module object. `module()` returns the `pybind11::module`, `load(name)` runs a registered loader once, `is_loaded(name)` tells whether it already ran.

### Reachable from Python (`sihd.<module>`)

- `util`: `Node`, `Named`, `Configurable`, `Node.ChildEntry`, `AService`, `ServiceController`, `IClock`, `SystemClock`, `SteadyClock`, `Waitable`, `Scheduler`, `Splitter`, `Timestamp`, `Duration`, `IArray` and typed arrays (`ArrChar` and the others); submodules `log`, `types`, `thread`, `time`.
- `sys`: `Uuid`, `ProcessInfo`, `Color`, `Bitmap`; submodules `fs` (`exists`, `is_file`, `is_dir`, `children`, `recursive_children`, `normalize`, `combine`, `remove_file`, ...), `os` (`pid`, `current_rss`, `peak_rss`, `max_fds`, `is_run_by_debugger`, ...), `user` (`is_root`, `name`), `signal` (`stop_received`, `should_stop`, `reset_received`, ...), `screenshot` (`take_screen`, `take_under_cursor`, `take_focused`, `take_window_name`).
- `core`: `Channel` (`write`, `write_array`, `read`, `copy_to`, `size`, `resize`, `set_observer`, `notify`), `Device` (`add_channel`, `setup`, `init`, `start`, `stop`, `reset`), `Core`, `ACoreObject`, `ChannelWaiter` (`observe`, `wait`, `wait_for`, `prev_wait`, `prev_wait_for`), `DevFilter`, `DevPulsation`, `DevSampler`, `DevPlayer`, `DevRecorder`, `DevMessage` (`set_message_path`, `set_trigger_mode`).
- `net`: `IpAddr` (`localhost`, `set_hostname`, `has_ip`, `is_ipv4`, `is_ipv6`, `port`, `str`, subnet functions), `DeviceTcpClient`, `DeviceTcpServer`, `DeviceUdpSender`, `DeviceUdpReceiver`.
- `http`: `Navigator` (configuration only: `set_*`, `clear_auth`, `clear_headers`, `cookies`, `save_cookies`, `load_cookies`, `set_proxy`; the request methods are not bound to Python), `RequestOptions` (read-write properties), `Multipart`, `Progress`, `HttpRequest`, `HttpResponse`, `NavigatorResponse`, `WebService`, `HttpServer` (`set_port`, `set_root_dir`, `set_cors_origin`, `request_stop`, `is_running`).

## Example

```cpp
#include <pybind11/embed.h>

void run_node_script()
{
    pybind11::scoped_interpreter guard{};
    pybind11::eval_file("test/util/py/test_node.py");
}
```

Adapted from `modules/py/test/util/TestPyUtilApi.cpp` (`test_pyutil_node`), which runs the script through one interpreter owned by a gtest environment (`modules/py/test/PyInterpreter.hpp`).

```python
import sihd

root = sihd.util.Node("root")
parent1 = sihd.util.Node("parent1", root)
named1 = sihd.util.Named("child", sihd.util.Node("sub_parent", parent1))

assert(root.find("parent1") == parent1)
assert(parent1.parent() == root)
```

Adapted from `modules/py/test/util/py/test_node.py`.

The `sihd` extension must be importable by the embedding interpreter (for example by putting the build's `lib` directory on `PYTHONPATH`); the test runner exports `LIB_PATH` for this. The embedding example above is not compiled or run here.

## Scripting

Exposed to Python as the `sihd` package (this module is the Python binding layer). Not exposed to Lua.
