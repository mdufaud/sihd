name = 'sihd'
version = "0.1.0"
git_url = "https://github.com/mdufaud/sihd.git"

###############################################################################
# external libs
###############################################################################

extlibs = {
    # unit test
    "gtest": "1.11.0",
    # json parsing
    "nlohmann_json": "3.9.1",
    # util
    "cxxopts": "3.0.0",
    "fmt": "7.1.3",
    "libuuid": "1.0.3",
    # http
    "libwebsockets": "4.3.0",
    "libcurl": "7.75.0",
    "openssl": "1.1.1l",
    "libssh": "",
    # pcap
    "libpcap": "1.10.1",
    # usb
    "libusb": "1.0.24",
    # gui
    "ftxui": "5.0.0",
    "sdl": "2.0.18",
    "glew": "2.2.0",
    "glfw": "3.3.6",
    # bindings
    "pybind11": "2.6.2",
    # compressing utility
    "libzip": "1.7.3",
    # other
    "libjpeg": "9d",
    "lua": "5.3.5",
    "libbluetooth": "",
}

conan_skip = ["libuuid", "lua", "libbluetooth"]
conan_post_process = {
    "*lua.*": {"from": "lua.", "to": "lua5.3."}
}

###############################################################################
# modules
###############################################################################

modules = {
    "util": {
        "extlibs": ['nlohmann_json', 'fmt', 'cxxopts'],
        "linux-extlibs": ['libuuid'],
        "libs": ['pthread'], # threading
        "linux-libs": [
            'dl', # dl_open...
            'rt', # shm_open...
            'uuid',
        ],
        # only link dynamically fmt when no cross compiling
        "linux-dyn-libs": [
            'fmt', # get from sys lib only when compiling dynamically on linux
            'stdc++fs'
        ],
        "static-defines": ["FMT_HEADER_ONLY"], # static linkage of fmt lib
        "windows-libs": [
            'rpcrt4', # Uuid
            'psapi', # GetModuleFileName/GetProcessMemoryInfo
            'ucrt', # timezone
            'ssp', # winsock
            'ws2_32', # windows api
            'stdc++fs'
        ],
        "windows-defines": ["FMT_HEADER_ONLY"], # static linkage of fmt lib
        "em-flags": ["-pthread"], # enable threads
        "em-link": [
            "-sFORCE_FILESYSTEM", # use filesystem
            "-sUSE_PTHREADS", # enable threads
            "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency", # use max cpu threads
            "-sPROXY_TO_PTHREAD" # main is a navigator thread
        ],
        "em-defines": ["FMT_HEADER_ONLY"], # static linkage of fmt lib
    },
    "core": {
        "depends": ['util'],
    },
    "net": {
        "depends": ['util'],
        "extlibs": ['openssl'],
        "libs": ['ssl', 'crypto'],
    },
    "http": {
        "depends": ['net'],
        "extlibs": ['libcurl', 'libwebsockets'],
        "libs": ['websockets', 'curl'],
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
    },
    "tui": {
        "depends": ['util'],
        "git-url": "https://github.com/ArthurSonzogni/FTXUI",
        "git-branch": f"v{extlibs['ftxui']}",
    },
    "ssh": {
        "depends": ['util'],
        "extlibs": ["libssh"],
        "libs": ['ssh'],
        "platforms": ["linux"]
    },
    "usb": {
        "depends": ['util'],
        "extlibs": ['libusb'],
        "libs": ['usb-1.0'],
    },
    "bt": {
        "depends": ['util'],
        "extlibs": ['libbluetooth'],
        "libs": ['bluetooth'],
        "platforms": ["linux"]
    },
    "csv": {
        "depends": ['util'],
    },
    "imgui": {
        # sdl=1 to compile with SDL2
        "depends": ['util'],
        "extlibs": ['glfw', 'glew'],
        "linux-libs": ['glfw', 'GLEW', 'GL'],
        "windows-libs": [
            "glfw3", "glew32", "opengl32",
            # desktop window manager api
            "dwmapi",
            # direct x11
            "d3d11",
            # shader compiler
            "d3dcompiler",
            # directx graphics infrastructure
            "dxgi",
            # graphics device interface
            "gdi32"
        ],
        "windows-flags": [
            "-Wno-cast-function-type",
            "-Wno-stringop-overflow"
        ],
        "git-url": "https://github.com/ocornut/imgui.git",
        "git-branch": "v1.86",
    },
}

# conditionnal modules - activated only if compiled explicitly or by env variable
conditionnal_modules = {
    "lua": {
        # apt liblua5.3-dev / pacman lua
        "extlibs": ['lua'],
        "depends": ['util'],
        "conditionnal-env": "lua",
        "conditionnal-depends": ['core', 'net', 'http'],
        "flags": ["-Wno-unused-parameter"],
        "pkg-configs": ["lua-5.3", "lua53"],
        "git-url": "git@github.com:kunitoki/LuaBridge3.git",
        "git-branch": "2.6",
    },
    "luabin": {
        "depends": ['lua'],
        "conditionnal-env": "lua",
        "flags": ["-Wno-unused-parameter"],
    },
    "py": {
        "depends": ['util'],
        "extlibs": ['pybind11'],
        "conditionnal-env": "py",
        "conditionnal-depends": ['core', 'net', 'http'],
        "clang-flags": ["-Wno-unused-command-line-argument"],
        # pip install python-config
        "parse-configs": [
            'python-config --cflags --ldflags --embed',
            'python3-config --cflags --ldflags --embed',
        ],
        "platforms": ["linux"]
    }
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
cxx_flags = ['-std=c++17']
static_defines = ['STATIC']

## mode specifics - default is always debug
modes = ["debug", "fast", "release"]

_default_flags = [
    "-D_FORTIFY_SOURCE=2"
]

debug_flags = _default_flags + ["-g", "-Og"]
fast_flags = ["-O0"]
release_flags = _default_flags + ["-O3"]

## gcc specifics

gcc_flags = [
    "-Werror",
    "-fasynchronous-unwind-tables",
    "-fexceptions",
    "-Wl,-pie",
    "-fstack-protector",
    "-fstack-protector-strong",
    "-Wl,-z,defs",
    "-Wl,-z,now",
    "-Wl,-z,relro",
    # hide pragma messages
    # "-ftrack-macro-expansion=0",
    # "-fno-diagnostics-show-caret",
]

## clang specifics

clang_flags = [
    "-Werror",
    "-Wno-unused-command-line-argument"
]
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
    "nlohmann_json": "nlohmann-json3-dev",
    "fmt": "libfmt-dev",
    "libuuid": "uuid-dev",
    "cxxopts": "libcxxopts-dev",
    "openssl": "openssl",
    "libcurl": "libcurl4-openssl-dev",
    "libwebsockets": "libwebsockets-dev",
    "libpcap": "libpcap-dev",
    "libssh": "libssh-dev",
    "libusb": "libusb-dev",
    "libzip": "libzip-dev",
    "libbluetooth": "libbluetooth-dev",
    "pybind11": "python3-pybind11",
    # apt libmesa-dev for opengl
    "glfw": "libglfw3-dev",
    "glew": "libglew-dev",
    "sdl2": "libsdl2-dev",
    "lua": "liblua5.3-dev",
    "libusb": "libusb-dev",
}

## PACMAN

# used to create PKGBUILD build command
additionnal_build_env = ['py', 'lua', 'sdl']
# source to clone and build
pacman_source = "{name}-{version}::git+{git_url}#tag={version}".format(
    name=name,
    version=version,
    git_url=git_url,
)
# packages equivalent to build PKGBUILD dependencies
pacman_packages = {
    "gtest": "gtest",
    "nlohmann_json": "nlohmann-json",
    "fmt": "fmt",
    "libuuid": "util-linux-libs",
    "cxxopts": "cxxopts",
    "openssl": "openssl",
    "libcurl": "curl",
    "libwebsockets": "libwebsockets",
    "libpcap": "libpcap",
    "libssh": "libssh",
    "libusb": "libusb",
    "libzip": "libzip",
    "libbluetooth": "bluez",
    "pybind11": "pybind11",
    "glfw": "glfw",
    "glew": "glew",
    "sdl2": "sdl2",
    "lua": "lua",
    "libusb": "libusb"
}

## YUM

yum_packages = {
    "gtest": "gtest-devel",
    "nlohmann_json": "json-devel",
    "fmt": "fmt-devel",
    "libuuid": "uuid-devel",
    "cxxopts": "cxxopts-devel",
    "openssl": "openssl",
    "libcurl": "libcurl-devel",
    "libwebsockets": "libwebsockets-devel",
    "libpcap": "libpcap-devel",
    "libssh": "libssh-devel",
    "libusb": "libusb-devel",
    "libzip": "libzip-devel",
    "libbluetooth": "bluez-libs-devel",
    "pybind11": "python3-pybind11",
    "glfw": "glfw-devel",
    "glew": "glew-devel",
    "sdl2": "sdl2-devel",
    "lua": "lua5.3-devel",
    "libusb": "libusb-devel",
}

## MSYS2

__msys2_mingw = "mingw-w64-x86_64-"
msys2_packages = {
    "gtest": f"{__msys2_mingw}gtest",
    "nlohmann_json": f"{__msys2_mingw}nlohmann-json",
    "fmt": f"{__msys2_mingw}fmt",
    "cxxopts": f"{__msys2_mingw}cxxopts",
    "openssl": f"{__msys2_mingw}openssl",
    "libcurl": f"{__msys2_mingw}curl",
    "libwebsockets": f"{__msys2_mingw}libwebsockets",
    "libpcap": f"{__msys2_mingw}libpcap",
    "libssh": f"{__msys2_mingw}libssh",
    "libusb": f"{__msys2_mingw}libusb",
    "libzip": f"{__msys2_mingw}libzip",
    "pybind11": f"{__msys2_mingw}pybind11",
    "glfw": f"{__msys2_mingw}glfw",
    "glew": f"{__msys2_mingw}glew",
    "sdl2": f"{__msys2_mingw}SDL2",
    "lua": f"{__msys2_mingw}lua",
    "libusb": f"{__msys2_mingw}libusb",
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
