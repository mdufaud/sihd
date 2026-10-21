# SIHD

[![ci](https://github.com/mdufaud/sihd/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/mdufaud/sihd/actions/workflows/ci.yml)
[![ci-nightly](https://github.com/mdufaud/sihd/actions/workflows/ci-nightly.yml/badge.svg?branch=master)](https://github.com/mdufaud/sihd/actions/workflows/ci-nightly.yml)

Simple Input Handler Displayer: a modular C++23 library to acquire, process and display data, with optional Lua and Python bindings.

It ships as independent libraries (`libsihd_<module>`): OS abstraction, networking, HTTP/WebSocket, SSH, crypto, packet capture, archives, terminal and ImGui front-ends, and a Device/Channel data-flow core. It builds for Linux, Windows, Android and WebAssembly, on x86_64, aarch64, arm32 and riscv64.

## Modules

| Module | Purpose |
|--------|---------|
| [`util`](docs/modules/util.md) | Logging, services, threads, scheduling, observers, CLI, time, containers |
| [`sys`](docs/modules/sys.md) | Files, processes, IPC, signals, clipboard, screenshot, input injection, users |
| [`core`](docs/modules/core.md) | Devices, Channels, recording/playback, sampling |
| [`json`](docs/modules/json.md) · [`csv`](docs/modules/csv.md) · [`zip`](docs/modules/zip.md) | Data formats and archives |
| [`crypto`](docs/modules/crypto.md) | Keys, certificates, TLS context, ciphers, digests |
| [`net`](docs/modules/net.md) · [`pcap`](docs/modules/pcap.md) | Sockets, TCP/UDP/ICMP, TLS, DNS, packet capture |
| [`curl`](docs/modules/curl.md) · [`http`](docs/modules/http.md) | HTTP/WebSocket server, HTTP client |
| [`ssh`](docs/modules/ssh.md) | SSH client and server, SFTP |
| [`bt`](docs/modules/bt.md) · [`usb`](docs/modules/usb.md) | Bluetooth LE scanning, USB (placeholder) |
| [`tui`](docs/modules/tui.md) · [`imgui`](docs/modules/imgui.md) | Terminal and Dear ImGui front-ends |
| [`lua`](docs/modules/lua.md) · [`luabin`](docs/modules/luabin.md) · [`py`](docs/modules/py.md) | Scripting bindings |

Full index with dependencies: [docs/modules.md](docs/modules.md).

## Quick start

```shell
git clone --recursive git@github.com:mdufaud/sihd.git && cd sihd
make setup && source .venv/bin/activate   # SCons in a local venv
make dep test=1                           # external libraries via vcpkg, inside the repo
make test=1                               # build every module and its tests
make itest                                # run the non-interactive tests
```

Work on a subset of modules (their sihd dependencies come along):

```shell
make dep m=util,net test=1
make m=util,net test=1
make itest m=util,net
```

Outputs land in `build/<machine>-<platform>-<libc>/<compiler>-<version>/<mode>/<shared|static>/`; `build/last` points to the latest one (`lib/`, `include/`, `bin/`, `test/`, `demo/`).

## Requirements

- A C++23 compiler: g++ 14 or newer, or a recent clang.
- Python 3 and SCons 4.11+ (`make setup` installs SCons in `.venv`).
- vcpkg tooling: `curl zip unzip tar cmake ninja-build`. vcpkg itself is cloned inside the repo by `make dep`; set `VCPKG_ROOT` to reuse an existing checkout.

To use system packages instead of vcpkg, list them with:

```shell
make pkgdep apt m=util,net test=1   # or pacman
```

## Using SIHD in your project

Install the build, then link the modules you use:

```shell
make mode=release
make install          # into /usr/local; INSTALL_PREFIX=/usr to change it, INSTALL_DESTDIR=pkg to stage
```

```cpp
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/str.hpp>

SIHD_NEW_LOGGER("myapp");

int main()
{
    sihd::util::LoggerManager::stream();
    for (const auto & word : sihd::util::str::split("hello sihd", " "))
        SIHD_LOG(info, "{}", word);
    return 0;
}
```

```shell
g++ -std=c++23 main.cpp -lsihd_util -lsihd_json -lfmt -o app
```

`make uninstall` removes what `make install` wrote (tracked in `.installed`). Packages: `make dist apt`, `make dist pacman`, `make dist tar`, or `make dist mod <modules> <type>` for a subset.

Each page in [docs/modules](docs/modules/) has an example. Demo programs are built with `demo=1` into `build/last/demo/`.

## Build options

Options combine freely:

| Option | Effect |
|--------|--------|
| `m=util,net` | Build only these modules |
| `test=1` / `demo=1` | Also build tests / demos |
| `mode=release` | Optimized, no debug symbols |
| `static=1` | Static libraries |
| `compiler=clang` | Use clang (`compiler=em` for emscripten, `compiler=zig`) |
| `asan=1` | Address sanitizer |
| `lua=1` / `py=1` | Build the Lua / Python bindings |
| `x11=1` / `wayland=1` | Linux desktop backends for `sys` (clipboard, screenshot, input) |

Cross compilation (fetch dependencies with the same options first, e.g. `make dep platform=win`):

```shell
make platform=win            # Windows (MinGW)
make platform=web            # WebAssembly (emscripten)
make machine=aarch64         # also riscv64, arm32
make libc=musl
```

Build system reference (SCons Build Tools): [site_scons/sbt/docs](site_scons/sbt/docs/).

## Tests

```shell
make itest m=util                  # non-interactive tests
make itest m=util t='*Logger*'     # gtest filter
make itest m=util repeat=30        # repeat to catch flakes
make itest jtest=4                 # run shards concurrently
make test ls                       # list tests
make iasantest                     # address sanitizer
make valtest                       # valgrind
make gdbtest                       # debug under gdb
```

## Continuous integration

| Workflow | Trigger | What it checks |
|----------|---------|----------------|
| [ci](https://github.com/mdufaud/sihd/actions/workflows/ci.yml) | every push and pull request on `master` | Ubuntu, gcc 14: all modules with tests and demos under ASan, `make iasantest`, and a compile of the X11/Wayland backends |
| [ci-nightly](https://github.com/mdufaud/sihd/actions/workflows/ci-nightly.yml) | daily, or manual | Full matrix: x64 gcc, clang and zig-musl; arm32, arm64 and riscv64 (shared and static); Windows MinGW (dynamic and static); WebAssembly; Android |

Latest runs: [GitHub Actions](https://github.com/mdufaud/sihd/actions).

## Repository layout

```
modules/<module>/
    include/sihd/<module>/   public headers
    src/                     implementation
    test/                    GoogleTest suites
    demo/                    demo programs
    scons.py                 module build script
app.py                       module list, dependencies, external libraries
docs/                        user documentation
site_scons/sbt/              build framework (git submodule)
```
