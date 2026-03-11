# Addon Guide

## What is addon?

`site_scons/addon/` is the **project-specific extension point** for the SBT build system.

SBT (`site_scons/sbt/`) is a generic, reusable build framework. `addon/` contains project-specific configuration that SBT loads automatically.

```
site_scons/
├── sbt/           # Generic build system
│   ├── scons.py
│   ├── builder.py
│   ├── vcpkg/
│   └── ...
└── addon/         # Project-specific extensions (stays in your repo)
    ├── distribution.py
    ├── makefile/
    └── vcpkg/
```

## Structure

```
site_scons/addon/
├── distribution.py          # Package manager metadata
├── makefile/
│   └── Makefile.custom      # Extra Make targets
└── vcpkg/
    ├── config.py            # Per-port cmake options, vcpkg flags
    ├── overlay-ports/       # Patched vcpkg ports
    └── triplets/            # Custom vcpkg triplet .cmake files (optional)
```

## How addon files are loaded

### Via app.py includes

```python
# app.py
includes = [
    "addon/vcpkg/config.py",
    "addon/distribution.py",
]
```

Paths are relative to `site_scons/`. The loader processes includes in two phases:

1. **Merge** — All public attributes (not `_`-prefixed) from the included file are copied into the `app` module
2. **Configure** — If the include defines `configure(app)`, it's called with the merged `app`

This means any variable you define in `addon/vcpkg/config.py` becomes accessible as `app.vcpkg_cflags`, `app.vcpkg_cmake_configure_options`, etc.

### Via automatic discovery

SBT detects addon directories automatically:

- **Overlay ports**: `addon/vcpkg/overlay-ports/` is passed to vcpkg as `--overlay-ports=...`
- **Custom triplets**: `addon/vcpkg/triplets/` is scanned for `.cmake` files and passed as `--overlay-triplets=...`
- **Custom Makefile**: `addon/makefile/Makefile.custom` is included by the main Makefile if it exists

## vcpkg/config.py

Per-port cmake options and vcpkg compiler flags. This is where you handle library build quirks.

### Compiler flags

Injected into **all** vcpkg port compilations via the overlay triplet:

```python
vcpkg_cflags = [
    "-Wno-error=discarded-qualifiers",       # Fix lib X with GCC 14
]
vcpkg_cxxflags = []
```

### Per-port cmake options

Base config (restrictive by default, safe for cross-compilation):

```python
vcpkg_cmake_configure_options = {
    "sdl3": ["-DSDL_X11=OFF", "-DSDL_WAYLAND=OFF"],
    "glfw3": ["-DGLFW_BUILD_X11=OFF"],
}
```

Platform overrides (re-enable features):

```python
# Native linux: empty list removes restrictions
vcpkg_cmake_configure_options_linux = {
    "sdl3": [],
    "glfw3": [],
}

# Cross-linux: selective re-enable (X11/Wayland from vcpkg)
vcpkg_cmake_configure_options_cross_linux = {
    "sdl3": ["-DSDL_IBUS=OFF", "-DSDL_JACK=OFF"],
    "glfw3": [],
}

# Windows (mingw)
vcpkg_cmake_configure_options_windows = {
    "curl": ["-DHAVE_IOCTLSOCKET_FIONBIO=ON"],
}

# Emscripten
vcpkg_cmake_configure_options_web = {
    "sdl3": ["-DSDL_OPENGLES=ON"],
}
```

Naming convention for variants:
- `vcpkg_cmake_configure_options` — base (always applied)
- `vcpkg_cmake_configure_options_{platform}` — platform-specific (native only)
- `vcpkg_cmake_configure_options_cross_{platform}` — cross-compilation for that platform

### Default features control

```python
vcpkg_no_default_features = [
    "dbus",    # systemd default breaks on musl
    "libusb",  # udev default needs system headers
]
```

These libs get `"default-features": false` in the vcpkg manifest during cross-builds.

### Cross-linux display packages

Extra packages to build from source when cross-compiling for linux (X11/Wayland):

```python
vcpkg_cross_linux_extlibs = {
    "libx11": "",
    "libxext": "",
    "libxkbcommon": "",
    "wayland": "",
    "wayland-protocols": "",
}

vcpkg_cross_linux_extlibs_features = {
    "wayland": ["force-build"],
    "wayland-protocols": ["force-build"],
}
```

These are installed in a 2-phase process for cross-linux builds (see [cross-compilation.md](cross-compilation.md)).

## vcpkg/overlay-ports/

Patched vcpkg ports that override the official ones. Each subdirectory is a complete vcpkg port:

```
addon/vcpkg/overlay-ports/
├── dbus/
│   ├── portfile.cmake
│   └── vcpkg.json
├── libcap/
│   ├── portfile.cmake
│   └── vcpkg.json
└── libxcursor/
    ├── portfile.cmake
    └── vcpkg.json
```

Passed to vcpkg as `--overlay-ports=site_scons/addon/vcpkg/overlay-ports`. Typical reasons to patch:
- Remove a dependency that doesn't build on a target (e.g., dbus without systemd)
- Fix cross-compilation variables (e.g., libcap OBJCOPY)
- Fix dependency resolution bugs

## vcpkg/triplets/

Custom triplet `.cmake` files for targets not covered by vcpkg built-in or SBT:

```
addon/vcpkg/triplets/
└── my-custom-triplet.cmake
```

Passed to vcpkg as `--overlay-triplets=site_scons/addon/vcpkg/triplets`. SBT statically scans this directory to discover available triplets.

Priority order for triplet resolution:
1. **SBT custom** (`sbt/vcpkg/triplets/`)
2. **vcpkg built-in** (`.vcpkg/triplets/`)
3. **vcpkg community** (`.vcpkg/triplets/community/`)
4. **Addon custom** (`addon/vcpkg/triplets/`)
5. **Dynamic** (generated from `architectures.py` for musl/zig)

## makefile/Makefile.custom

Extra Make targets included by the main Makefile:

```makefile
# addon/makefile/Makefile.custom

.PHONY: my-custom-target
my-custom-target:
	@echo "Running custom target"
	# your commands here
```

## Creating a new addon file

### Adding a new include

1. Create your file in `addon/`:

```python
# addon/my_config.py
my_setting = "value"

def configure(app):
    # Called after all includes are merged
    # Access app.name, app.version, etc.
    pass
```

2. Register it in `app.py`:

```python
includes = [
    "addon/vcpkg/config.py",
    "addon/distribution.py",
    "addon/my_config.py",        # new
]
```

### Adding a custom overlay port

1. Copy the port from `.vcpkg/ports/<name>/` to `addon/vcpkg/overlay-ports/<name>/`
2. Edit `portfile.cmake` or `vcpkg.json` as needed
3. vcpkg automatically picks it up via `--overlay-ports`

### Adding a custom triplet

1. Create `addon/vcpkg/triplets/<triplet-name>.cmake`
2. SBT discovers it automatically through filesystem scan
