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
    "json": {
        "extlibs": ['simdjson'],
        "export-libs": ['simdjson'],
    },
    "util": {
        "depends": ['json'],
        "linux-libs": ['pthread'], # threading (bionic has it in libc)
        "windows-libs": ['pthread'], # mingw winpthread
        "extlibs": ['fmt'],
        "export-libs": ['fmt'],
        # stdc++fs only needed for GCC < 9 with glibc
        "mingw-gnu-libs": ['stdc++fs'],
        # === Android specific ===
        "android-libs": ['log'],
        # === Emscripten specific ===
        "em-flags": [
            "-pthread", # enable threads
            "-Wno-deprecated-literal-operator",
            "-Wno-unknown-pragmas",
            "-Wno-deprecated-declarations",
        ],
        "export-all-flags": True,
        "em-link": [
            "-sFORCE_FILESYSTEM", # use filesystem
            "-sUSE_PTHREADS=1", # enable threads
            "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency", # use max cpu threads
            "-sPROXY_TO_PTHREAD", # main is a narrator thread
        ],
    },
    "sys": {
        "depends": ['util'],
        "env": {
            "x11": 0, # x11=1 to compile with X11
            "wayland": 0, # wayland=1 to compile with Wayland
        },
        "export-all-libs": True,
        # === Linux specific ===
        "linux-extlibs": ['libuuid'],
        "linux-libs": [
            'dl', # dlopen
            'uuid', # libuuid
        ],
        # === Android specific ===
        "android-extlibs": ['libuuid'],
        "android-libs": ['log', 'android', 'uuid'],
        # === Windows specific ===
        "windows-libs": [
            'rpcrt4', # Uuid
            'psapi', # GetProcessMemoryInfo
            'ssp', # winsock (Poll)
            'ws2_32', # windows socket api (Poll)
            'gdi32', # wingdi (screenshot/clipboard)
            'imagehlp', # backtrace / SymFromAddr
            # ! never add libucrt with mingw
        ],
    },
    "core": {
        "depends": ['util', 'sys'],
    },
    "net": {
        "depends": ['util', 'sys'],
        "extlibs": ['openssl'],
        "libs": ['ssl', 'crypto'],
        "export-libs": ['ssl', 'crypto'],
        "export-windows-libs": [
            'crypt32',
            'bcrypt',
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
        "linux-extlibs": ["libcap"],
        # all libs are platform-specific due to different names and link order requirements
        "linux-libs": ["websockets", "curl", "z", "uv", "cap"],
        # Windows static linking: all transitive deps must be explicit
        # order matters: higher-level libs first, their deps after
        "windows-libs": [
            'websockets_static', # vcpkg builds libwebsockets_static.a on mingw
            'curl',              # libcurl
            'uv',                # libuv (libwebsockets uses it)
            'zlib',              # vcpkg zlib installs libzlib.a on mingw
            'winmm',             # Multimedia (curl)
            'iphlpapi',          # IP Helper (libwebsockets, libuv)
            'userenv',           # User environment (libwebsockets, libuv)
            'advapi32',          # Advanced API (curl, libuv)
            'user32',            # User interface (libuv)
            'dbghelp',           # Debug (libuv)
            'ole32',             # COM (libuv)
            'uuid',              # UUID (libuv)
        ],
    },
    "pcap": {
        "depends": ['net'],
        "extlibs": ['libpcap'],
        "libs": ['pcap'],
        # if lib is installed on system
        "parse-configs": [
            "pcap-config --cflags --libs",
        ],
        # Windows static linking: libpcap needs winsock
    },
    "zip": {
        "depends": ['util', 'sys'],
        "extlibs": ['libzip'],
        "libs": ['zip'],
    },
    "tui": {
        "depends": ['util', 'sys'],
        "extlibs": ['ftxui'],
        "libs": ["ftxui-component", "ftxui-dom", "ftxui-screen"],
    },
    "ssh": {
        "depends": ['util', 'sys'],
        "extlibs": ["libssh"],
        "libs": ['ssh'],
        # libssh.a depends on OpenSSL (needed for cross static linking)
        "linux-cross-libs": ['ssl', 'crypto'],
        "windows-libs": [
            'ssh',               # libssh
            'ssl', 'crypto',     # OpenSSL (libssh dep)
            'crypt32',           # CryptoAPI (OpenSSL)
            'bcrypt',            # BCrypt (OpenSSL)
            'iphlpapi',          # if_nametoindex (libssh)
        ],
    },
    "usb": {
        "depends": ['util', 'sys'],
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
        "platforms": ["linux"],
        "depends": ['util', 'sys'],
        "extlibs": ['simpleble'],
        "libs": ['simpleble'],
        # dbus-1 is a transitive dep of simpleble on Linux (needed for static linking)
        "linux-libs": ['dbus-1'],
    },
    "csv": {
        "depends": ['util', 'sys'],
    },
    "imgui": {
        "depends": ['util', 'sys'],
        "extlibs": ['imgui', 'opengl'],
        # libxcrypt only supports linux|osx (autotools, no Windows/web port)
        "linux-extlibs": ['libxcrypt'],
        "libs": ["imgui"],
        # native linux: system provides libglfw.so, libGLEW.so, libGL.so
        "linux-native-libs": ['glfw', 'GLEW', 'GL', 'SDL3'],
        # cross linux: vcpkg provides libglfw.so (dynamic) / libglfw3.a (static)
        # (imgui has its own GL loader via dlopen, GLEW is unused)
        "linux-cross-libs": ['glfw', 'SDL3'],
        # === Android specific ===
        "android-libs": ['android', 'EGL', 'GLESv3', 'log'],
        "android-defines": ['IMGUI_IMPL_OPENGL_ES3'],
        # === Web/Emscripten specific ===
        "web-libs": ['SDL3'],
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
            "-sWASM=1",
            "-sALLOW_MEMORY_GROWTH=1", # dynamic heap growth
        ],
        "em-defines": ["IMGUI_IMPL_OPENGL_ES2"],
    },
}

# conditional modules - activated only if compiled explicitly or by env variable
conditional_modules = {
    "lua": {
        "conditional-env": "lua",
        "extlibs": ['lua', 'luabridge3'],
        "depends": ['util', 'sys'],
        "conditional-depends": ['core'],
        "libs": ["lua"],
        "flags": ["-Wno-unused-parameter", "-Wno-unused-but-set-parameter"],
        "pkg-configs": ["lua-5.3", "lua53"],
    },
    "luabin": {
        "conditional-env": "lua",
        "depends": ['lua'],
        "libs": ["lua"],
        "flags": ["-Wno-unused-parameter"],
    },
    "py": {
        "conditional-env": "py",
        "platforms": ["linux"],
        "extlibs": ['pybind11', 'python3'],
        "depends": ['util', 'sys'],
        "conditional-depends": ['core'],
        "libs": ["python3.12"],
        "flags": ['-U_FORTIFY_SOURCE', '-Wno-cpp'], # Undefine _FORTIFY_SOURCE for py module since it requires optimization from builds using -O0
        "clang-flags": ["-Wno-unused-command-line-argument"],
    }
}

###############################################################################
# cppcheck
###############################################################################

cppcheck = {
    "undefines": ["SIHD_LOGGING_OFF"],
}

###############################################################################
# external libs
###############################################################################

extlibs = {
    # unit test
    "gtest": "1.17.0#2",
    # json parsing
    "simdjson": "4.2.2",
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
    "imgui": ["glfw-binding", "opengl3-binding", "sdl3-binding", "win32-binding", "dx11-binding"],
}

extlibs_features_android = {
    "imgui": ["android-binding", "opengl3-binding"],
}

extlibs_features_web = {
    "imgui": ["sdl3-binding", "opengl3-binding"],
}

# on windows some libs are not available through vcpkg
extlibs_skip_windows = [
    "dbus",
    "libcap",
    "simpleble",
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
    "opengl",
    "glfw3",
    "dbus",
    "simpleble",
    "libcap",
]

# on android: libs that are linux-only or not relevant
extlibs_skip_android = [
    "dbus",
    "libcap",
    "libwebsockets",
    "curl",
    "libssh",
    "libpcap",
    "libusb",
    "libxcrypt",
    "ftxui",
    "opengl",
    "glfw3",
    "simpleble",
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

gcc_size_link = ['-s']
gcc_release_link = ['-s']

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

clang_size_link = ['-s']
clang_release_link = ['-s']

## mingw specifics

mingw_flags = gcc_flags

## emscripten specifics

em_link = ["--emrun"]
em_size_link = ['--strip-debug']
em_release_link = ['--strip-debug']

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
    # Use the vcpkg Python version for the extension suffix, not the system one.
    # python3-config --extension-suffix returns the system Python suffix which may
    # differ from the vcpkg-provided Python version we actually link against.
    major_minor = "312"
    try:
        proc = subprocess.Popen("python3-config --extension-suffix",
                                shell=True, stdout=subprocess.PIPE)
        suffix = proc.stdout.read().decode().strip()
        if suffix:
            # Replace the version in the suffix (e.g. cpython-314 -> cpython-312)
            import re
            suffix = re.sub(r'cpython-\d+', 'cpython-{}'.format(major_minor), suffix)
            return suffix
    except Exception:
        pass
    return ""


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
