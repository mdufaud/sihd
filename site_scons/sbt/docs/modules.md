# Modules Guide

## What is a module?

A module is a self-contained build unit (library, binary, tests, demos) with this layout:

```
mymodule/
├── scons.py                    # Build script (required)
├── include/project/mymodule/   # Public headers
├── src/                        # Source files
├── test/                       # Unit tests
│   └── main.cpp
└── demo/                       # Demo programs (optional)
    └── etc/                    # Demo resources (optional)
```

## Minimal scons.py

```python
Import('env')

lib = env.build_lib(Glob('src/*.cpp'))
test = env.build_test(Glob('test/*.cpp'), add_libs=[env.module_format_name()])

Return('lib')
```

`Return('lib')` is required — it returns the library node to the build system.

## Declaring a module

Modules are declared in `app.py`. A minimal module with no dependencies:

```python
modules = {
    "mymodule": {},
}
```

A module depending on others:

```python
modules = {
    "mymodule": {
        "depends": ['util', 'core'],
        "libs": ['pthread'],
    },
}
```

SBT auto-fills missing keys (`depends`, `libs`, `link`, `flags`) as empty lists.

## Module configuration keys

### Core keys

| Key | Type | Description |
|-----|------|-------------|
| `depends` | `list[str]` | Internal module dependencies (built before this module) |
| `libs` | `list[str]` | System/external libraries to link (`-l<name>`) |
| `flags` | `list[str]` | Compiler flags (`CPPFLAGS`) |
| `defines` | `list[str]` | Preprocessor defines (`-D<name>`) |
| `link` | `list[str]` | Linker flags (`LINKFLAGS`) |
| `extlibs` | `list[str]` | External library names (vcpkg), see [dependencies.md](dependencies.md) |

### Discovery keys

| Key | Type | Description |
|-----|------|-------------|
| `pkg-configs` | `list[str]` | pkg-config packages to query (native builds only) |
| `parse-configs` | `list[str]` | Shell commands for config tools (native builds only) |

Both are **skipped in cross-compilation** to avoid host header pollution. In cross builds, headers/libs come from vcpkg's `extlib/` directory instead.

```python
"usb": {
    "parse-configs": ["pkg-config libusb-1.0 --cflags --libs"],
    # ^ only runs on native builds
    "cross-libs": ['usb-1.0'],
    # ^ used when cross-compiling (vcpkg provides the lib)
},
```

### Platform restriction

| Key | Type | Description |
|-----|------|-------------|
| `platforms` | `list[str]` | Whitelist of platforms (`linux`, `windows`, `web`, `android`). Omit = all platforms. |

```python
"bt": {
    "platforms": ["linux"],  # bluetooth only on linux
},
```

### Module-level env defaults

| Key | Type | Description |
|-----|------|-------------|
| `env` | `dict[str, any]` | Default values for CLI/env variables |

```python
"sys": {
    "env": {
        "x11": 0,      # default off, enable with x11=1
        "wayland": 0,   # default off, enable with wayland=1
    },
},
```

These values are set in `os.environ` at load time. The module's `scons.py` reads them with `env.is_opt("x11")`.

### Inheritance keys

| Key | Type | Description |
|-----|------|-------------|
| `export-libs` | `list[str]` | Export module libraries to dependent modules |
| `export-defines` | `list[str]` | Export preprocessor defines to dependent modules |
| `export-flags` | `list[str]` | Export compiler flags to dependent modules |
| `export-all-libs` | `bool` | Export all active direct `libs` and variant `*-libs` of the module |
| `export-all-defines` | `bool` | Export all active direct `defines` and variant `*-defines` of the module |
| `export-all-flags` | `bool` | Export all active direct `flags` and variant `*-flags` of the module |
| `export-all-link` | `bool` | Export all active direct `link` and variant `*-link` of the module |
| `export-<variant>-libs` | `list[str]` | Export variant-specific libraries such as `export-windows-libs` |
| `export-<variant>-defines` | `list[str]` | Export variant-specific defines |
| `export-<variant>-flags` | `list[str]` | Export variant-specific compiler flags such as `export-em-flags` |
| `inherit-depends-defines` | `bool` | Inherit CPPDEFINES from dependencies |
| `inherit-depends-links` | `bool` | Inherit LINKFLAGS from dependencies |
| `inherit-depends-flags` | `bool` | Inherit CPPFLAGS from dependencies |
| `inherit-depends-generated-libs` | `bool` | Inherit generated library names from dependencies (default: `True`) |

### Android keys

| Key | Type | Description |
|-----|------|-------------|
| `android-permissions` | `list[str]` | Android manifest permissions |

## Platform/compiler variant keys

Any core key can be prefixed with a variant selector:

```
{variant}-{key}
```

Where `{variant}` is one or a combination of:

| Variant | Values | Example |
|---------|--------|---------|
| Platform | `linux`, `windows`, `web`, `android` | `linux-libs` |
| Compiler | `gcc`, `clang`, `mingw`, `em`, `ndk`, `zig` | `em-flags` |
| Lib type | `static`, `dyn` | `static-defines` |
| Mode | `debug`, `fast`, `size`, `release` | `debug-flags` |
| Libc | `gnu`, `musl` | `musl-flags` |
| Cross | `native`, `cross` | `native-libs`, `cross-libs` |

**Combinations** — two variants can be combined:

```python
"linux-static-libs": ['z'],          # linux + static
"linux-native-libs": ['udev'],       # linux + native build
"linux-cross-libs": ['usb-1.0'],     # linux + cross build
"gcc-debug-flags": ['-Og'],          # gcc + debug
"mingw-gnu-libs": ['stdc++fs'],      # mingw + gnu libc
```

All combinations of `(platform, libtype, mode, compiler, libc, native/cross)` are valid. SBT iterates over all of them at build time.

### Example: full module

```python
"http": {
    "depends": ['net'],
    "extlibs": ['libwebsockets', 'curl', 'zlib', 'libuv'],
    "linux-extlibs": ["libcap"],
    "linux-libs": ["websockets", "curl", "z", "uv", "cap", "ssl", "crypto"],
    "windows-libs": [
        'websockets_static', 'curl', 'ssl', 'crypto',
        'uv', 'zlib', 'ws2_32', 'crypt32', 'bcrypt',
    ],
},
```

## Environment methods available in scons.py

### Build methods

| Method | Description |
|--------|-------------|
| `env.build_lib(sources)` | Build shared/static library. Returns lib node. |
| `env.build_bin(sources, name=, add_libs=[])` | Build executable binary |
| `env.build_test(sources, add_libs=[])` | Build test binary (only if `test=1`) |
| `env.build_demo(source, name=, add_libs=[], android_dir=None)` | Build demo (only if `demo=1`) |
| `env.build_demos(sources)` | Build multiple demos (one per source file) |
| `env.build_cpp_modules(sources, module_name=, imports=[])` | Build GCC C++20 module interfaces and register them for later imports |

When a target imports named modules, pass them explicitly with `cpp_modules=[...]`:

```python
cpp_modules = env.build_cpp_modules('test/modules/TestGreeting.cppm')

env.build_test(
    Glob('test/*.cpp') + cpp_modules,
    add_libs=[env.module_format_name()],
    cpp_modules=['sihd.util.test.greeting'],
)
```

This does three things:

1. enables `-fmodules` on the importer build,
2. wires SCons dependencies so imported interfaces are compiled first,
3. links the interface object together with the importer target.

### Module info

| Method | Returns |
|--------|---------|
| `env.module_name()` | Module name: `"core"` |
| `env.module_format_name()` | Formatted lib name: `"project_core"` |
| `env.module_conf()` | Module config dict from app.py |
| `env.module_dir()` | Absolute path to module directory |
| `env.modules_to_build()` | List of all modules being built |
| `env.builder()` | Access to builder object (platform, paths, etc.) |
| `env.app()` | Access to app.py module |

### Configuration queries

| Method | Returns |
|--------|---------|
| `env.is_opt(name)` | `True` if CLI arg / env var is `"1"` or `"true"` |
| `env.get_opt(name, default="")` | Get CLI arg / env var value |

### Dynamic library discovery

| Method | Description |
|--------|-------------|
| `env.pkg_config(name)` | Run pkg-config for a package (native only) |
| `env.parse_config(cmd)` | Parse config tool output (native only) |

### File operations

| Method | Description |
|--------|-------------|
| `env.copy_into_build(src, dst)` | Copy files into build directory |
| `env.git_clone(url, branch, dest)` | Clone a git repo |
| `env.build_replace(patterns, replacements)` | Replace strings in build files |
| `env.file_replace(path, replacements)` | Replace strings in a file |
| `env.find_in_file(path, string)` | Search for string in file |
| `env.file_basename(path)` | Extract filename without extension |
| `env.filter_files(files, filter_list)` | Filter out files by name |
| `env.create_module_env(**kwargs)` | Create a sub-environment from current module config |

### Builder object

```python
builder = env.builder()

builder.build_platform     # "linux", "windows", "web", "android"
builder.build_machine      # "x86_64", "arm64", "arm32", ...
builder.build_compiler     # "gcc", "clang", "mingw", "em", "ndk", "zig"
builder.build_mode         # "debug", "fast", "size", "release"
builder.build_static_libs  # True/False
builder.libc               # "gnu", "musl"
builder.build_tests        # True/False
builder.build_demo         # True/False
builder.build_path         # Full build path
builder.build_lib_path     # lib/ directory
builder.build_bin_path     # bin/ directory

builder.is_cross_building()   # True if host != target
builder.get_gnu_triplet()     # e.g. "aarch64-linux-gnu"
```

## scons.py patterns

### Platform-conditional sources

```python
Import('env')

builder = env.builder()

srcs = ["src/Common.cpp"]

if builder.build_platform == "android":
    srcs.append("src/AndroidBackend.cpp")
elif builder.build_platform == "windows":
    srcs.append("src/WindowsBackend.cpp")
elif builder.build_platform != "web":
    srcs.append("src/LinuxBackend.cpp")

lib = env.build_lib(srcs)
Return('lib')
```

### Optional features via env vars

```python
Import('env')

if env.is_opt("x11"):
    env.Append(LIBS=["X11"])
    env.Append(CPPDEFINES=["COMPILE_WITH_X11"])

lib = env.build_lib(Glob('src/*.cpp'))
Return('lib')
```

### Multiple demos from a directory

```python
Import('env')

lib = env.build_lib(Glob('src/*.cpp'))

for src in Glob('demo/*.cpp'):
    name = env.file_basename(src)
    env.build_demo(src, name=name, add_libs=[env.module_format_name()])

test = env.build_test(Glob('test/*.cpp'), add_libs=[env.module_format_name()])
Return('lib')
```

### Conditional compilation by modules present

```python
Import('env')

modules = env.modules_to_build()

srcs = [Dir("src").File("Base.cpp")]
tests = [Dir("test").File("main.cpp")]

for module in modules:
    if module == "mymodule":
        continue
    srcs += Glob(f"src/{module}/*.cpp")
    tests += Glob(f"test/{module}/*.cpp")

lib = env.build_lib(srcs)
test = env.build_test(tests, add_libs=[env.module_format_name()])
Return('lib')
```

## Conditional modules

Modules that are only built when explicitly requested.

```python
# app.py
conditional_modules = {
    "lua": {
        "conditional-env": "lua",           # activated by lua=1
        "depends": ['util', 'sys'],
        "conditional-depends": ['core'],    # depends on core only if core is being built
        "extlibs": ['lua', 'luabridge3'],
        "libs": ["lua"],
    },
}
```

| Key | Description |
|-----|-------------|
| `conditional-env` | Env var name that activates this module when set to `1` |
| `conditional-depends` | Dependencies that apply only if those modules are in the build |

### Activation

```bash
# Via env var
make lua=1

# Via explicit module selection (also works)
make m=lua
```

When built via `m=lua`, the module's full dependency tree (`util`, `sys`) is resolved automatically.

## Module dependencies

Dependencies are resolved recursively. If `http` depends on `net` which depends on `util`:

```bash
make m=http
# → builds util, sys, net, http (in dependency order)
```

Modules are sorted by dependency depth and built in order.

## Build output

```
build/<machine>-<platform>-<libc>/<compiler>-<version>/<mode>/
├── lib/           # Libraries (.so/.a/.dll)
├── bin/           # Binaries
├── demo/          # Demo executables
├── include/       # Copied public headers
├── test/bin/      # Test binaries
├── obj/           # Intermediate object files
│   └── mymodule/
├── etc/           # Copied config files
├── share/         # Copied share files
└── extlib/        # Symlink → vcpkg installed packages
```

`build/last` symlinks to the most recent build.

## CLI reference

```bash
make                           # Build all modules
make m=util,core               # Build specific modules
make test=1                    # Build + compile tests
make demo=1                    # Build demos
make mode=debug                # Debug build
make mode=release              # Release build
make static=1                  # Static libraries
make v=1                       # Verbose output
make asan=1                    # Address sanitizer
make compiler=clang            # Use clang
make def=MY_DEFINE=1           # Add preprocessor define
```

### Testing

```bash
make test m=core               # Build + run tests
make itest                     # Non-interactive tests
make vtest                     # Tests with valgrind
make ttest                     # Tests with strace
make stest                     # Tests with sanitizer
```
