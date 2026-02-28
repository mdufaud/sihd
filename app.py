import sys

name = 'sihd'
version = "0.1.0"
git_url = "https://github.com/mdufaud/sihd.git"

# files to include into this module (paths relative to site_scons/)
includes = [
    "addon/vcpkg/config.py",
    "addon/distribution.py",
]

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
        # z/bz2/lzma/ssl/crypto: transitive deps of libzip (needed for static linking on native)
        "linux-native-libs": ['z', 'bz2', 'lzma', 'ssl', 'crypto'],
        # cross builds use dynamic vcpkg libs; libzip.so links its own transitive deps
        "linux-cross-libs": ['z', 'bz2', 'ssl', 'crypto'],
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
        # cross linux: vcpkg provides libglfw.so (dynamic) / libglfw3.a (static)
        # (imgui has its own GL loader via dlopen, GLEW is unused)
        "linux-cross-libs": ['glfw', 'SDL3'],
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

# _WIN64 -> activates sihd functionalities
# _WIN32_WINNT -> activates higher version of WIN functionalities (mingw)
# NTDDI_VERSION -> activates higher version of WIN functionalities (mingw)
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
