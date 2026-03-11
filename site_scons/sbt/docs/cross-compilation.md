# Cross-Compilation Guide

## Supported targets

| Target | Command | Compiler | Triplet |
|--------|---------|----------|---------|
| Native x86_64 | `make` | gcc | `x64-linux` |
| Clang | `make compiler=clang` | clang | `x64-linux` |
| Musl x86_64 | `make libc=musl` | x86_64-linux-musl-gcc | `x64-linux-musl` |
| aarch64 | `make machine=aarch64` | aarch64-linux-gnu-gcc | `arm64-linux` |
| arm32 | `make machine=arm32` | arm-linux-gnueabihf-gcc | `arm-linux` |
| riscv64 | `make machine=riscv64` | riscv64-linux-gnu-gcc | `riscv64-linux` |
| Windows | `make platform=windows` | x86_64-w64-mingw32-gcc | `x64-mingw-dynamic` |
| Emscripten | `make compiler=em` | emcc/em++ | `wasm32-emscripten-threads` |
| Android | `make platform=android` | NDK clang | `arm64-android` |

## When is it cross-compilation?

```python
is_cross_building() = (host_machine != build_machine)
                   or (host_libc != libc)
                   or (host_platform != build_platform)
```

- `machine=aarch64` on x86_64 → different machine
- `libc=musl` on glibc host → different libc
- `platform=windows` on linux → different platform
- `compiler=em` → forces `platform=web`
- `platform=android` → different platform

## Platform/compiler-specific module configuration

### Variant keys

Any module config key can be prefixed with a selector. See [modules.md](modules.md) for the full table.

```python
"mymodule": {
    # Base
    "libs": ['mylib'],

    # Platform
    "linux-libs": ['pthread'],
    "windows-libs": ['ws2_32'],
    "android-libs": ['log'],

    # Compiler
    "gcc-flags": ['-Wno-specific-warning'],
    "em-flags": ['-pthread'],
    "em-link": ['-sUSE_PTHREADS=1'],
    "mingw-gnu-libs": ['stdc++fs'],

    # Native vs Cross
    "native-libs": ['udev'],
    "cross-libs": ['usb-1.0'],
    "linux-native-libs": ['z', 'bz2', 'lzma'],
    "linux-cross-libs": ['z', 'bz2'],
}
```

### How variants are resolved

SBT builds a list of variant selectors for the current build:

```python
# Always present:
[platform, libtype, mode, compiler, libc]
# e.g.: ["linux", "dyn", "fast", "gcc", "gnu"]

# Plus cross/native:
["cross", "linux-cross"]  # or ["native", "linux-native"]
```

For each selector and key (`libs`, `flags`, `link`, `defines`), SBT looks up `{selector}-{key}`. All 2-way combinations are also checked (e.g., `linux-static-libs`, `gcc-debug-flags`).

### Platform-specific sources in scons.py

```python
Import('env')
builder = env.builder()

srcs = ["src/Common.cpp"]

if builder.build_platform == "android":
    srcs.append("src/Android.cpp")
elif builder.build_platform == "web":
    srcs.append("src/Web.cpp")
elif builder.build_platform == "windows":
    srcs.append("src/Windows.cpp")
else:
    srcs.append("src/Linux.cpp")

lib = env.build_lib(srcs)
Return('lib')
```

## Compiler details

### GCC (native + cross)

Prefix resolved from `architectures.py`:

| Machine | Libc | Prefix |
|---------|------|--------|
| x86_64 | gnu | *(none — native)* |
| arm64 | gnu | `aarch64-linux-gnu-` |
| arm32 | gnu | `arm-linux-gnueabihf-` |
| riscv64 | gnu | `riscv64-linux-gnu-` |
| x86_64 | musl | `x86_64-linux-musl-` |
| arm64 | musl | `aarch64-linux-musl-` |

Install cross-compilers:
```bash
# Debian/Ubuntu
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
sudo apt install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu
# Musl
sudo apt install musl-tools  # x86_64 only
```

### Clang

Uses `--target=<gnu-triplet>` instead of a prefix:

```bash
clang --target=aarch64-linux-gnu -c main.cpp
```

Same cross-compilation sysroot as GCC (needs the cross-gcc packages installed for headers/libs).

### MinGW (Linux → Windows)

Fixed prefix: `x86_64-w64-mingw32-`. Always static linking.

```bash
sudo apt install mingw-w64
```

Gotchas:
- Library names differ: `websockets_static` instead of `websockets`, `zlib` instead of `z`
- Link order matters more (all transitive deps must be explicit)
- After build, DLLs are copied from the cross toolchain to `bin/`

### Emscripten (WebAssembly)

Compilers: `emcc` / `em++`. Config read from `~/.emscripten`.

```bash
# Install emsdk
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source emsdk_env.sh
```

Constraints:
- Always static linking
- Pthreads: `-pthread` at compile, `-sUSE_PTHREADS=1` at link
- Limited module support (no networking/system libs)
- Binary output is `.html` + `.js` + `.wasm` (+ `.worker.js` with threads)

### NDK (Android)

Compiler: NDK clang with versioned target triplet (e.g., `aarch64-linux-android24-clang++`).

```bash
# Set NDK path
export ANDROID_NDK_PATH=/opt/android-sdk/ndk/28.0.13004108
```

| `machine=` | ABI | NDK prefix |
|------------|-----|------------|
| arm64 | arm64-v8a | `aarch64-linux-android24-` |
| arm32 | armeabi-v7a | `armv7a-linux-androideabi24-` |
| x86_64 | x86_64 | `x86_64-linux-android24-` |

Constraints:
- Always static linking (`.so` only for final APK)
- No RPATH (Android doesn't use it)
- No SONAME versioning
- Uses `-static-libstdc++`

### Zig

Single `zig cc` command for cross-compilation. Forces musl libc.

```bash
make compiler=zig machine=aarch64
```

## Musl libc

```bash
make libc=musl                    # x86_64
make libc=musl machine=aarch64    # aarch64
```

Gotchas:
- Adds `-static-libgcc -static-libstdc++` (host libstdc++ is glibc-linked)
- Several libs are excluded from linking (built into musl): `pthread`, `m`, `dl`, `rt`, `crypt`, `util`, `xnet`, `resolv`
- vcpkg `dbus` default features disabled (systemd doesn't build on musl)
- Triplets generated dynamically from `architectures.py` — no static `.cmake` needed

## Host header protection

### The problem

In cross-compilation, host tools like `pkg-config` return host paths (`-I/usr/include`). If added to the cross-compiler command, host headers (x86_64 glibc) get included by the cross compiler (aarch64), causing type conflicts and compilation failures.

### Solution 1: parse-configs disabled in cross

`pkg-configs` and `parse-configs` are **silently skipped** when `builder.is_cross_building()` returns `True`. vcpkg provides all headers/libs in `extlib/` instead.

### Solution 2: cmake cross toolchain for vcpkg

The overlay triplet generates a cmake toolchain that isolates the cross-compiler:

```cmake
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

This prevents cmake (building vcpkg ports) from finding host headers.

## vcpkg in cross-compilation

### Per-port cmake options

Strategy: **restrictive by default, permissive on native, selective on cross**.

Three config levels in `addon/vcpkg/config.py`:

```python
# 1. Base: disable system-dependent features (safe for any target)
vcpkg_cmake_configure_options = {
    "sdl3": ["-DSDL_X11=OFF", "-DSDL_WAYLAND=OFF"],
}

# 2. Native linux: re-enable everything (system provides X11, Wayland)
vcpkg_cmake_configure_options_linux = {
    "sdl3": [],   # empty = remove base restrictions
}

# 3. Cross-linux: selective re-enable (X11/Wayland from vcpkg)
vcpkg_cmake_configure_options_cross_linux = {
    "sdl3": ["-DSDL_IBUS=OFF", "-DSDL_JACK=OFF"],
}
```

Resolution: native applies `_linux`; cross applies base then merges with `_cross_linux`.

### Cross-linux 2-phase install (X11/Wayland)

**Problem**: Ports like `glfw3`/`sdl3` use cmake's `FindX11`/`FindWayland` which search the sysroot — not vcpkg deps. In cross-linux, no X11/Wayland in the sysroot.

**Solution**: Two-phase vcpkg install:

1. **Phase 1**: Install only X11/Wayland foundation packages (`vcpkg_cross_linux_extlibs`)
2. **Phase 2**: Install full manifest — `FindX11` now finds headers from phase 1

Phase 1 is skipped if:
- Not cross-building for linux
- No module needs display libs (auto-detected from module libs)
- Foundation packages already installed (sentinel: `libX11.so` exists)

### Overlay triplet

Generated in `build/<path>/vcpkg/overlay-triplets/`. It wraps the original triplet and adds:
- Extra compiler flags (`vcpkg_cflags`, `vcpkg_cxxflags`)
- Cross cmake toolchain
- Cross meson file (for ports using meson, e.g., dbus)
- Per-port cmake options

### Gotcha: VCPKG_CXX_FLAGS vs VCPKG_CMAKE_CONFIGURE_OPTIONS

`VCPKG_CXX_FLAGS` is injected by vcpkg's own toolchains (`scripts/toolchains/linux.cmake`). But:
- **No emscripten toolchain exists** in vcpkg → `VCPKG_CXX_FLAGS` is ignored
- Emscripten triplets must use `VCPKG_CMAKE_CONFIGURE_OPTIONS` instead

SBT's custom emscripten triplet handles this.

## Transitive static linking

In cross-compilation, vcpkg libs are often static (`.a`). When linking a shared library (`.so`) against a `.a`, **all transitive deps must be explicit**.

```python
"http": {
    "extlibs": ['libwebsockets', 'curl', 'zlib', 'libuv'],
    # Linux: all transitive deps for static linking
    "linux-libs": ["websockets", "curl", "z", "uv", "cap", "ssl", "crypto"],
},
"zip": {
    "extlibs": ['libzip'],
    "libs": ['zip'],
    # Different transitive deps for native vs cross
    "linux-native-libs": ['z', 'bz2', 'lzma', 'ssl', 'crypto'],
    "linux-cross-libs": ['z', 'bz2', 'ssl', 'crypto'],
},
```

**Tip**: Use `readelf -d lib.so | grep NEEDED` to check what a shared lib actually needs. Use `nm --undefined-only lib.a` to find missing symbols.

## Adding support for a new architecture

### gcc-based

1. Add entry to `site_scons/sbt/architectures.py`:

```python
architectures = {
    "myarch": {
        "gcc": {"gnu": "myarch-linux-gnu-", "musl": "myarch-linux-musl-"},
        "vcpkg": "myarch",
        "meson": {"cpu_family": "myarch", "cpu": "myarch", "endian": "little"},
    },
}
```

2. If vcpkg doesn't have a built-in triplet, create one in `sbt/vcpkg/triplets/myarch-linux-dynamic.cmake` or `addon/vcpkg/triplets/`.

3. Musl and Zig triplets are generated automatically from `architectures.py`.

### Machine aliases

```python
machine_aliases = {
    "aarch64": "arm64",
    "amd64": "x86_64",
    "armv7l": "arm32",
}
```

These allow `make machine=aarch64` to resolve to the `arm64` architecture entry.

## Build paths

Each cross-target gets its own build directory:

```
build/
├── x86_64-linux-gnu/gcc-15/fast/      # native
├── x86_64-linux-gnu/clang-21/fast/    # native clang
├── x86_64-linux-musl/gcc-15/fast/     # musl
├── arm64-linux-gnu/gcc-15/fast/       # aarch64 cross
├── riscv64-linux-gnu/gcc-15/fast/     # riscv64 cross
├── x86_64-windows-gnu/mingw-15/fast/  # windows cross
├── x86_64-web-gnu/em-5/fast/          # emscripten
└── arm64-android-bionic/ndk-28/fast/  # android
```

## Quick reference

```bash
# Get dependencies for cross target
make dep machine=aarch64
make dep libc=musl
make dep platform=windows

# Build
make machine=aarch64
make libc=musl
make platform=windows
make compiler=em m=util,core
make platform=android m=imgui static=1 demo=1

# Test (native only — can't run cross binaries)
make test

# Serve WASM demo
make serve_demo
```
