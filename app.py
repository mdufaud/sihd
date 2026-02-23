import sys

name = 'sihd'
version = "0.1.0"
git_url = "https://github.com/mdufaud/sihd.git"

###############################################################################
# modules
###############################################################################

modules = {
    "util": {
        "env": {
            "x11": 0, # x11=1 to compile with X11
            "wayland": 0, # wayland=1 to compile with Wayland
        },
        "libs": ['pthread'], # threading
        "extlibs": ['nlohmann-json', 'fmt'],
        # === Linux specific ===
        "linux-extlibs": ['libuuid'],
        "linux-libs": [
            'dl', # dl_open...
            'uuid',
        ],
        # === Windows specific ===
        "windows-libs": [
            'rpcrt4', # Uuid
            'psapi', # GetModuleFileName/GetProcessMemoryInfo
            'ssp', # winsock
            'ws2_32', # windows socket api
            'gdi32', # wingdi
            'imagehlp', # backtrace / SymFromAddr
            # ! never add libucrt with mingw
        ],
        # stdc++fs only needed for GCC < 9 with glibc
        "mingw-gnu-libs": ['stdc++fs'],
        # === Emscripten specific ===
        "em-flags": [
            "-pthread", # enable threads
            "-Wno-deprecated-literal-operator",
            "-Wno-unknown-pragmas",
            "-Wno-deprecated-declarations",
        ],
        "em-link": [
            "-sFORCE_FILESYSTEM", # use filesystem
            "-sUSE_PTHREADS=1", # enable threads
            "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency", # use max cpu threads
            "-sPROXY_TO_PTHREAD", # main is a navigator thread
        ],
    },
    "core": {
        "depends": ['util'],
    },
    "ipc": {
        "depends": ['util'],
    },
    "net": {
        "depends": ['util'],
        "extlibs": ['openssl'],
        "libs": ['ssl', 'crypto'],
        "windows-libs": [
            'psapi', # GetModuleFileName/GetProcessMemoryInfo
            'ssp', # winsock
            'imagehlp', # backtrace / SymFromAddr
            'ws2_32', # windows socket api
        ],
    },
    "http": {
        "depends": ['net'],
        "extlibs": [
            'libwebsockets',
            'curl',
            'zlib',
            'libuv',
        ],
        "libs": [
            'websockets',
            'curl',
            'z',
            'uv',
        ],
        "linux-extlibs": ["libcap"],
        # ssl/crypto: transitive deps of libwebsockets & curl (needed for static linking)
        "linux-libs": ["cap", "ssl", "crypto"],
    },
    "pcap": {
        "depends": ['net'],
        "extlibs": ['libpcap'],
        "libs": ['pcap'],
        # if lib is installed on system
        "parse-configs": [
            "pcap-config --cflags --libs",
        ],
    },
    "zip": {
        "depends": ['util'],
        "extlibs": ['libzip'],
        "libs": ['zip'],
        # z/bz2/lzma/ssl/crypto: transitive deps of libzip (needed for static linking)
        "linux-libs": ['z', 'bz2', 'lzma', 'ssl', 'crypto'],
    },
    "tui": {
        "depends": ['util'],
        "extlibs": ['ftxui'],
        "libs": ["ftxui-component", "ftxui-dom", "ftxui-screen"],
    },
    "ssh": {
        "depends": ['util'],
        "extlibs": ["libssh"],
        "libs": ['ssh'],
        # libssh.a depends on OpenSSL (needed for cross static linking)
        "linux-cross-libs": ['ssl', 'crypto'],
    },
    "usb": {
        "depends": ['util'],
        "extlibs": ['libusb'],
        # native linux: udev is a system transitive dep of libusb, parse-config provides libusb flags
        "native-libs": ['udev'],
        "parse-configs": [
            "pkg-config libusb-1.0 --cflags --libs",
        ],
        # cross linux: vcpkg libusb built without udev, parse-configs skipped in cross
        "cross-libs": ['usb-1.0'],
    },
    "bt": {
        "depends": ['util'],
        "extlibs": ['simpleble'],
        "libs": ['simpleble'],
        # dbus-1 is a transitive dep of simpleble on Linux (needed for static linking)
        "linux-libs": ['dbus-1'],
    },
    "csv": {
        "depends": ['util'],
    },
    "imgui": {
        "depends": ['util'],
        "extlibs": ['imgui', 'libxcrypt', 'opengl'],
        "libs": ["imgui"],
        # native linux: system provides libglfw.so, libGLEW.so, libGL.so
        "linux-native-libs": ['glfw', 'GLEW', 'GL', 'SDL3'],
        # cross linux: vcpkg provides libglfw3.a (not libglfw.a), no GLEW/GL needed
        # (imgui has its own GL loader via dlopen, GLEW is unused)
        "linux-cross-libs": ['glfw3', 'SDL3'],
        "windows-link": [
            "-mwindows" # no shell window opening
        ],
        "windows-libs": [
            "glfw3",
            "opengl32",
            # SDL NEEDED
            "SDL3",
            "kernel32",
            "user32",
            "gdi32", # doubled
            "winmm",
            "imm32",
            "ole32",
            "oleaut32",
            "version",
            "uuid",
            "advapi32",
            "setupapi",
            "shell32",
            "dinput8",
            # END
            # desktop window manager api
            "dwmapi",
            # direct x11
            "d3d11",
            # shader compiler
            "d3dcompiler",
            # directx graphics infrastructure
            "dxgi",
            # graphics device interface
            "gdi32",
            # winsock
            'ssp',
        ],
        "em-flags": [
            "-pthread", # enable threads
            "-Wno-deprecated-literal-operator",
            "-Wno-unknown-pragmas",
            "-Wno-deprecated-declarations",
            "-sDISABLE_EXCEPTION_CATCHING=1",
        ],
        "em-link": [
            "-sUSE_PTHREADS=1", # enable threads
            "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency", # use max cpu threads
            # "-sUSE_GLFW=3", # did not manage to get glfw working with emscripten
            "-sWASM=1",
        ],
    },
}

# conditional modules - activated only if compiled explicitly or by env variable
conditional_modules = {
    "lua": {
        # apt liblua5.3-dev / pacman lua
        "extlibs": ['lua', 'luabridge3'],
        "depends": ['util'],
        "libs": ["lua"],
        "conditional-env": "lua",
        "conditional-depends": ['core'],
        "flags": ["-Wno-unused-parameter", "-Wno-unused-but-set-parameter"],
        "pkg-configs": ["lua-5.3", "lua53"],
    },
    "luabin": {
        "depends": ['lua'],
        "conditional-env": "lua",
        "flags": ["-Wno-unused-parameter"],
    },
    "py": {
        "platforms": ["linux"],
        "depends": ['util'],
        "extlibs": ['pybind11'],
        "conditional-env": "py",
        "conditional-depends": ['core'],
        "flags": ['-U_FORTIFY_SOURCE', '-Wno-cpp'], # Undefine _FORTIFY_SOURCE for py module since it requires optimization from builds using -O0
        "clang-flags": ["-Wno-unused-command-line-argument"],
        # pip install python-config
        "parse-configs": [
            'python-config --cflags --ldflags --embed',
            'python3-config --cflags --ldflags --embed',
        ],
    }
}

def add_fmt_to_modules(modules_list):
    for module in modules_list.values():
        # static linkage of fmt lib for all libs
        if "defines" in module:
            module["defines"].append("FMT_HEADER_ONLY")
        else:
            module["defines"] = ["FMT_HEADER_ONLY"]

add_fmt_to_modules(modules)
add_fmt_to_modules(conditional_modules)

###############################################################################
# external libs
###############################################################################

extlibs = {
    # unit test
    "gtest": "1.17.0#2",
    # json parsing
    "nlohmann-json": "3.12.0#1",
    # util
    "cli11": "2.6.1",
    "fmt": "12.1.0",
    "libuuid": "1.0.3#15",
    # http
    "libwebsockets": "4.5.2",
    "curl": "7.87.0",
    "openssl": "3.1.0",
    "libssh": "0.10.6",
    "libcap": "2.70",
    "libuv": "1.46.0",
    "zlib": "1.3.1",
    # pcap
    "libpcap": "1.10.5",
    # usb
    "libusb": "1.0.27",
    # gui
    "opengl": "",  # provides GL/gl.h headers for cross-compilation (via opengl-registry)
    "ftxui": "6.1.9",
    "imgui": "1.91.9",
    "libxcrypt": "4.5.2", #fixes a compilation issue with imgui
    # bindings
    "python3": "3.12.9",
    "pybind11": "3.0.1",
    # compressing utility
    "libzip": "1.7.3",
    # other
    "libjpeg": "9d",
    "lua": "5.3.5-5",
    "luabridge3": "3.0-rc3",
    # bt
    "simpleble": "0.8.1#1",
}

# glfw needs: libxi-dev libxinerama-dev libxcursor-dev xorg libglu1-mesa pkg-config
extlibs_features = {
    "imgui": ["glfw-binding", "opengl3-binding", "sdl3-binding"],
}

extlibs_features_linux = {
    "libusb": ["udev"],  # udev is linux-only
}

extlibs_features_windows = {
    "imgui": ["win32-binding", "dx11-binding"],
}

# on windows some libs are not available through vcpkg
extlibs_skip_windows = [
    "libwebsockets",
    "libssh",
    "libpcap",
    "libxcrypt"
]

# on web: those libs don't compile properly with emscripten threading
extlibs_skip_web = [
    "openssl",
    "libwebsockets",
    "curl",
    "libssh",
    "libpcap",
    "libusb",
    "libxcrypt",
    "ftxui",
    "imgui",
]

vcpkg_baseline = "3a3285c4878c7f5a957202201ba41e6fdeba8db4"

vcpkg_cflags = [
    "-Wno-error=discarded-qualifiers",   # libwebsockets v4.5.2 + GCC >= 14
]
vcpkg_cxxflags = []

# default config for cross compilation - to override with specific conf
vcpkg_cmake_configure_options = {
    "dbus": [
        "-DDBUS_SESSION_SOCKET_DIR=/tmp",
    ],
    # SDL3: disable system-dependent features by default.
    "sdl3": [
        "-DSDL_X11=OFF", "-DSDL_WAYLAND=OFF", "-DSDL_IBUS=OFF",
        "-DSDL_JACK=OFF", "-DSDL_PULSEAUDIO=OFF", "-DSDL_PIPEWIRE=OFF",
        "-DSDL_SNDIO=OFF", "-DSDL_KMSDRM=OFF",
        "-DSDL_OPENGL=OFF", "-DSDL_OPENGLES=OFF",
        "-DSDL_LIBUDEV=OFF", "-DSDL_LIBURING=OFF",
    ],
    # glfw3 enables X11 by default on Linux (REQUIRED) which fails without X11 headers
    "glfw3": ["-DGLFW_BUILD_X11=OFF"],
    # imgui: prevent glfw3.h from including GL/gl.h (imgui has its own GL3 loader)
    "imgui": ["-DCMAKE_CXX_FLAGS_INIT=-DGLFW_INCLUDE_NONE"],
}

# re-enable system features on native Linux (X11/Wayland/etc. from system packages)
vcpkg_cmake_configure_options_linux = {
    "sdl3": [],
    "glfw3": [],
    "imgui": [],
}

# cross-linux: re-enable X11/Wayland (headers now from vcpkg).
# Audio backends (JACK, PulseAudio...), ibus, udev etc. remain disabled (no vcpkg ports).
vcpkg_cmake_configure_options_cross_linux = {
    "sdl3": [
        # X11 and Wayland: enabled (built from vcpkg)
        # OpenGL: enabled (headers from opengl vcpkg package)
        "-DSDL_IBUS=OFF",
        "-DSDL_JACK=OFF", "-DSDL_PULSEAUDIO=OFF", "-DSDL_PIPEWIRE=OFF",
        "-DSDL_SNDIO=OFF", "-DSDL_KMSDRM=OFF",
        "-DSDL_OPENGLES=OFF",
        "-DSDL_LIBUDEV=OFF", "-DSDL_LIBURING=OFF",
        "-DSDL_WAYLAND_LIBDECOR=OFF",  # no vcpkg port for libdecor
        # Pre-set HAVE_XGENERICEVENT: the check_c_source_compiles test fails because
        # static libX11.a needs transitive deps (libxcb, libXau...) for linking, but
        # SDL3's cmake only passes libX11.a. The headers ARE correct.
        "-DHAVE_XGENERICEVENT=1",
    ],
    "glfw3": [],  # X11 from vcpkg, use GLFW defaults (X11=ON)
    "imgui": ["-DCMAKE_CXX_FLAGS_INIT=-DGLFW_INCLUDE_NONE"],
}

# Libraries whose default-features should be disabled in vcpkg manifest (cross-compilation only).
# Only explicitly listed features from extlibs_features will be used.
vcpkg_no_default_features = [
    "libusb",  # udev default requires libudev headers (no vcpkg port for libudev exists)
]

# Additional vcpkg packages needed only for cross-linux builds.
# On native Linux, X11/Wayland come from system packages.
# On cross-Linux (aarch64, riscv64, arm32...), vcpkg builds them from source
# using X_VCPKG_FORCE_VCPKG_X_LIBRARIES and X_VCPKG_FORCE_VCPKG_WAYLAND_LIBRARIES.
vcpkg_cross_linux_extlibs = {
    # X11 core + extensions
    "libx11": "",       # core X11 (transitive: xcb, xproto, xtrans, libxau, libxdmcp, xorg-macros)
    "libxext": "",      # X11 extension library (Xshape, Xshm...)
    "libxi": "",        # X Input extension (gamepad, touch...)
    "libxinerama": "",  # multi-monitor support
    "libxcursor": "",   # cursor management (needed by GLFW3, dlopen'd at runtime)
    "libxrandr": "",    # resolution/rotation (transitive: libxrender, libxfixes)
    "libxkbcommon": "", # keyboard handling (needed by both X11 and Wayland)
    # Wayland
    "wayland": "",            # core Wayland (requires force-build for cross)
    "wayland-protocols": "", # Wayland protocol extensions (requires force-build for cross)
}

vcpkg_cross_linux_extlibs_features = {
    "wayland": ["force-build"],
    "wayland-protocols": ["force-build"],
}

###############################################################################
# compilation
###############################################################################

## general compilation parameters

flags = ['-Wall', '-Wextra', '-pipe', '-fPIC']
defines = [
    "SIHD_VERSION_MAJOR=" + version.split('.')[0],
    "SIHD_VERSION_MINOR=" + version.split('.')[1],
    "SIHD_VERSION_PATCH=" + version.split('.')[2],
]
cxx_flags = ['-std=c++20']
static_defines = ['SIHD_STATIC']

## mode specifics
modes = ["debug", "fast", "size", "release"]

_default_flags = [
    "-D_FORTIFY_SOURCE=2",
]

debug_flags = _default_flags + ["-g", "-Og"]
fast_flags = ["-O0"]
size_flags = _default_flags + ["-Os"]
release_flags = _default_flags + ["-O3"]

## gcc specifics

gcc_flags = [
    "-Werror",
    "-fasynchronous-unwind-tables",
    "-fexceptions",
    "-fstack-protector",
    "-fstack-protector-strong",
    "-Wno-unknown-pragmas",
    # "-fconcepts-diagnostics-depth=2",/
    # hide pragma messages
    # "-ftrack-macro-expansion=0",
    # "-fno-diagnostics-show-caret",
]

gcc_link = [
    "-Wl,-z,defs",
    "-Wl,-z,now",
    "-Wl,-z,relro",
]

# Position Independent Executable for security hardening - only for binaries
gcc_dyn_bin_link = [
    "-Wl,-pie",
]

if hasattr(sys.stdout, 'isatty') and sys.stdout.isatty():
    gcc_flags.append("-fdiagnostics-color=always")

## clang specifics

clang_flags = [
    "-Werror",
    "-Wno-unused-command-line-argument",
    "-Wno-unknown-pragmas",
]
# clang uses libstdc++ by default on Linux, libc++ on macOS
# use clang-musl-libs if you need different libs for musl
clang_libs = ['stdc++']
clang_defines = [
    'LLVM_ENABLE_EH=YES',
    'LLVM_ENABLE_RTTI=ON',
]

## mingw specifics

mingw_flags = gcc_flags

## emscripten specifics

em_link = ["--emrun"]

## windows specifics

# _WIN64 -> activates sihd functionnalities
# _WIN32_WINNT -> activates higher version of WIN functionnalities (mingw)
# NTDDI_VERSION -> activates higher version of WIN functionnalities (mingw)
windows_defines = [
    "_WIN64",
    "_WIN32_WINNT=0x0600",
    "NTDDI_VERSION=0x06000000",
    "_ISOC99_SOURCE",
]

## test specifics

test_extlibs = ['gtest']
test_libs = ['gtest']

## demo specifics

demo_extlibs = ['cli11']
demo_libs = ['CLI11']

###############################################################################
# distribution
###############################################################################

description = "Simple Input Handler Displayer"
license = "GPL3"
section = "libdevel"
priority = "optional"
architecture = "any"
multi_architecture = "same"
maintainers = ["mdufaud <maxence_dufaud@hotmail.fr>"]
contributors = ["azouiten <alexandre.zouiten1@gmail.com>"]

## APT

# packages equivalent to build DEBIAN/control dependencies
apt_packages = {
    "gtest": "libgtest-dev",
    "nlohmann-json": "nlohmann-json3-dev",
    "fmt": "libfmt-dev",
    "libuuid": "uuid-dev",
    "cli11": "cli11-dev",
    "openssl": "openssl",
    "curl": "libcurl4-openssl-dev",
    "libwebsockets": "libwebsockets-dev",
    "libpcap": "libpcap-dev",
    "libssh": "libssh-dev",
    "libusb": "libusb-1.0-0-dev",
    "libzip": "libzip-dev",
    "simpleble": "libsimpleble-dev",
    "pybind11": "python3-pybind11",
    # apt libmesa-dev for opengl
    "glfw3": "libglfw3-dev",
    "glew": "libglew-dev",
    "sdl3": "libsdl3-dev",
    "lua": "liblua5.3-dev",
    "libusb": "libusb-dev",
    "x11": "libx11-dev",
    "wayland": "libwayland-dev",
}

## PACMAN

# used to create PKGBUILD build command
additionnal_build_env = ['py', 'lua', 'sdl', 'x11', 'wayland']
# source to clone and build
pacman_source = "{name}-{version}::git+{git_url}#tag={version}".format(
    name=name,
    version=version,
    git_url=git_url,
)
# packages equivalent to build PKGBUILD dependencies
pacman_packages = {
    "gtest": "gtest",
    "nlohmann-json": "nlohmann-json",
    "fmt": "fmt",
    "libuuid": "util-linux-libs",
    "cli11": "cli11",
    "openssl": "openssl",
    "curl": "curl",
    "libwebsockets": "libwebsockets",
    "libpcap": "libpcap",
    "libssh": "libssh",
    "libusb": "libusb",
    "libzip": "libzip",
    "simpleble": "simpleble",
    "pybind11": "pybind11",
    "glfw3": "glfw",
    "glew": "glew",
    "sdl3": "sdl3",
    "lua": "lua",
    "libusb": "libusb",
    "x11": "libx11",
    "wayland": "wayland",
}

## YUM

yum_packages = {
    "gtest": "gtest-devel",
    "nlohmann-json": "json-devel",
    "fmt": "fmt-devel",
    "libuuid": "uuid-devel",
    "cli11": "cli11-devel",
    "openssl": "openssl",
    "curl": "libcurl-devel",
    "libwebsockets": "libwebsockets-devel",
    "libpcap": "libpcap-devel",
    "libssh": "libssh-devel",
    "libusb": "libusb-devel",
    "libzip": "libzip-devel",
    "simpleble": "simpleble-devel",
    "pybind11": "python3-pybind11",
    "glfw3": "glfw-devel",
    "glew": "glew-devel",
    "sdl3": "sdl3-devel",
    "lua": "lua5.3-devel",
    "libusb": "libusb-devel",
    "x11": "libx11-devel",
    "wayland": "libwayland-devel",
}

## MSYS2

__msys2_mingw = "mingw-w64-x86_64-"
msys2_packages = {
    "gtest": f"{__msys2_mingw}gtest",
    "nlohmann-json": f"{__msys2_mingw}nlohmann-json",
    "fmt": f"{__msys2_mingw}fmt",
    "cli11": f"{__msys2_mingw}cli11",
    "openssl": f"{__msys2_mingw}openssl",
    "curl": f"{__msys2_mingw}curl",
    "libwebsockets": f"{__msys2_mingw}libwebsockets",
    "libpcap": f"{__msys2_mingw}libpcap",
    "libssh": f"{__msys2_mingw}libssh",
    "libusb": f"{__msys2_mingw}libusb",
    "libzip": f"{__msys2_mingw}libzip",
    "pybind11": f"{__msys2_mingw}pybind11",
    "glfw3": f"{__msys2_mingw}glfw",
    "glew": f"{__msys2_mingw}glew",
    "sdl3": f"{__msys2_mingw}sdl3",
    "lua": f"{__msys2_mingw}lua",
    "libusb": f"{__msys2_mingw}libusb",
    "x11": f"{__msys2_mingw}libx11",
    "wayland": f"{__msys2_mingw}wayland",
}

###############################################################################
# after build
###############################################################################

# linux extension is something like -> cpython-3Xm-x86_64-linux-gnu.so
# windows extension -> cp3X-win_amd64.pyd
# though python cannot be used with mingw


def __get_python_libname():
    import subprocess
    proc = subprocess.Popen("python3-config --extension-suffix",
                            shell=True, stdout=subprocess.PIPE)
    return proc.stdout.read().decode().strip()


def on_build_success(build_modules, builder):
    if "py" not in build_modules:
        return
    # for pybind11 shared lib to be usable by python interpreter
    # it needs to be like lib*.cpython-VER-ARCH-VENDOR-OS.so
    python_libname = __get_python_libname()
    if not python_libname:
        return
    import os
    import glob
    import shutil
    lib_pattern = os.path.join(builder.build_lib_path, "libsihd_py*.so*")
    for lib in glob.glob(lib_pattern):
        pybind11_compliant = lib.replace("libsihd_py", "sihd")
        pybind11_compliant = pybind11_compliant.replace(".so", python_libname)
        shutil.copyfile(lib, pybind11_compliant)
