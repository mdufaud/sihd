name = 'sihd'
version = "0.1"

# external libs versions
extlibs = {
    ## unit test
    "gtest": "1.11.0",
    ## json parsing
    "nlohmann_json": "3.9.1",
    ## crypt
    "openssl": "1.1.1i",
    ## http
    "libwebsockets": "4.3.0",
    "libcurl": "7.75.0",
    "openssl": "1.1.1l",
    ## ssh
    "libssh2": "1.9.0",
    ## pcap
    "libpcap": "1.10.1",
    ## usb
    "libusb": "1.0.24",
    ## gui
    "sdl": "2.0.18",
    "glew": "2.2.0",
    "glfw": "3.3.6",
    "ncurses": "6.2",
    ## command line parser
    "replxx": "0.0.4",
    ## bindings
    "pybind11": "2.6.2",
    "lua": "5.3.5",
    "sol2": "3.2.3",
    ## compressing utility
    "libzip": "1.7.3",
    ## other
    "libjpeg": "9d",
}

# modules descriptions
modules = {
    "util": {
        # apt nlohmann-json-dev / pacman nlohmann-json
        "use-extlibs": ['nlohmann_json'],
    },
    "core": {
        "depends": ['util'],
        "add-depends-libs": True,
    },
    "net": {
        "depends": ['util'],
        "add-depends-libs": True,
    },
    "zip": {
        # apt libzip-dev / pacman libzip
        "depends": ['util'],
        "add-depends-libs": True,
        "use-extlibs": ['libzip'],
        "libs": ['zip'],
    },
    "ssh": {
        # apt libssh-dev / pacman libssh
        "depends": ['util'],
        "add-depends-libs": True,
        "libs": ['sihd_util', 'ssh'],
    },
    "http": {
        # apt libwebsockets-dev / pacman libwebsockets
        "depends": ['net'],
        "add-depends-libs": True,
        "use-extlibs": ['openssl', 'libcurl', 'libwebsockets'],
        "libs": ['curl', 'websockets', 'ssl', 'crypto'],
    },
    "pcap": {
        # apt libpcap-dev / pacman libpcap
        "depends": ['net'],
        "add-depends-libs": True,
        "use-extlibs": ['libpcap'],
        "libs": ['pcap'],
    },
    "usb": {
        # apt libusb-dev / pacman libusb
        "depends": ['util'],
        "add-depends-libs": True,
        "use-extlibs": ['libusb'],
        "libs": ['usb'],
    },
    "bt": {
        # apt libbluetooth-dev
        "depends": ['util'],
        "add-depends-libs": True,
        "libs": ['bluetooth'],
    },
    "csv": {
        "depends": ['util'],
        "add-depends-libs": True,
    },
    "emscripten": {
        "depends": ['util'],
        "add-depends-libs": True,
    },
    "imgui": {
        # apt libmesa-dev for opengl
        # apt libglew-dev / pacman glew
        # apt libglfw-dev / pacman glfw
        # apt libsdl2-dev / pacman sdl2
        "depends": ['util'],
        "add-depends-libs": True,
        "use-extlibs": ['glfw', 'glew'],
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
        "windows-flags": ["-Wno-cast-function-type"],
        "git-url": "https://github.com/ocornut/imgui.git",
        "git-branch": "v1.86",
    },
}

# conditionnal modules - activated only if compiled specifically or by env variable
conditionnal_modules = {
    "demo": {
        "depends": ['pcap', 'http', 'imgui'],
        "add-depends-libs": True,
        "conditionnal-env": "demo",
    },
    "lua": {
        # apt liblua5.3-dev / pacman lua
        "depends": ['util'],
        "add-depends-libs": True,
        "use-extlibs": ['sol2'],
        "conditionnal-env": "lua",
        "conditionnal-depends": ['core', 'net', 'http'],
        "flags": ["-Wno-unused-parameter"],
        "pkg-configs": ["lua-5.3", "lua53"],
    },
    "luabin": {
        "depends": ['lua'],
        "add-depends-libs": True,
        "conditionnal-env": "lua",
        "flags": ["-Wno-unused-parameter"],
    },
    "py": {
        # apt libpython-dev / pacman python
        "depends": ['util'],
        "add-depends-libs": True,
        "use-extlibs": ['pybind11'],
        "conditionnal-env": "py",
        "conditionnal-depends": ['core', 'net', 'http'],
        # pip install python-config
        "parse-configs": [
            'python-config --cflags --ldflags --embed',
            'python3-config --cflags --ldflags --embed',
        ],
    }
}

#############
# replace
#############

replace_files = [
    "etc/sihd/core/test.txt"
]
replace_vars = {
    "VERSION": "0.1"
}

#############
# distribution
#############

description = "Simple Input Handler Displayer"
license = "GPL3"
section = "libdevel"
priority = "optional"
architecture = "any"
multi_architecture = "same"
maintainer = "mdufaud <maxence_dufaud@hotmail.fr>"
uploaders = "azouiten <alexandre.zouiten1@gmail.com>"
source = "https://github.com/mdufaud/sihd.git"

app_build_env = ['py', 'lua', 'sdl']

# packages equivalent to build DEBIAN/control dependencies
apt_packages = {
    "nlohmann_json": "nlohmann-json3-dev",
    "openssl": "openssl",
    "libcurl": "libcurl4-openssl-dev",
    "libwebsockets": "libwebsockets-dev",
    "libpcap": "libpcap-dev",
    "libssh": "libssh-dev",
    "pybind11": "python3-pybind11",
    "sol2": "",
    "glfw": "libglfw-dev",
    "glew": "libglew-dev",
    "sdl2": "libsdl2-dev",
}

# packages equivalent to build PKGBUILD dependencies
pacman_packages = {
    "nlohmann_json": "nlohmann-json",
    "openssl": "openssl",
    "libcurl": "libcurl",
    "libwebsockets": "libwebsockets",
    "libpcap": "libpcap",
    "libssh": "libssh",
    "pybind11": "pybind11",
    "sol2": "",
    "glfw": "glfw",
    "glew": "glew",
    "sdl2": "sdl2",
}

#############
# compilation
#############

## general compilation parameters
libs = ['pthread', 'dl']
flags = ['-Wall', '-Wextra', '-pipe', '-fPIC']
defines = []

## mode specifics
debug_flags = ["-g", "-O2"]
release_flags = ["-O3"]

## gcc specifics
gcc_flags = [
    "-Werror",
    "-D_FORTIFY_SOURCE=2",
    "-D_GLIBCXX_ASSERTIONS",
    "-fasynchronous-unwind-tables",
    "-fexceptions",
    "-Wl,-pie",
    "-fstack-protector",
    "-fstack-protector-strong",
    "-Wl,-z,defs",
    "-Wl,-z,now",
    "-Wl,-z,relro",
]

## clang specifics
clang_flags = ["-Werror"]
clang_libs = ['stdc++', "libc++"]
clang_defines = [
    'LLVM_ENABLE_EH=YES',
    'LLVM_ENABLE_RTTI=ON',
]

## mingw specifics
mingw_flags = ["-Werror"]
mingw_libs = ['ws2_32', 'psapi']
# _WIN64 -> activates sihd functionnalities
# _WIN32_WINNT -> activates higher version of WIN functionnalities (mingw)
mingw_defines = ["_WIN64", "_WIN32_WINNT=0x0600"]

## test specifics
# apt libgtest-dev / pacman gtest
test_libs = ['gtest', 'stdc++fs']

#############
# after build
#############

# linux extension is -> cpython-36m-x86_64-linux-gnu.so
# windows extension is -> cp37-win_amd64.pyd
# though python cannot be used with mingw so...
def __get_python_libname():
    import subprocess
    proc = subprocess.Popen("python3-config --extension-suffix",
                            shell = True, stdout = subprocess.PIPE)
    return proc.stdout.read().decode().strip()

def on_build_success(build_modules, builder_helper):
    if "py" not in build_modules:
        return
    # -> copy python library to another name in build so we can use it
    python_libname = __get_python_libname()
    if not python_libname:
        return
    import os
    import glob
    import shutil
    lib_pattern = os.path.join(builder_helper.build_lib_path, "libsihd_py*.so")
    for lib in glob.glob(lib_pattern):
        pybind11_compliant = lib.replace("libsihd_py", "sihd")
        pybind11_compliant = pybind11_compliant.replace(".so", python_libname)
        shutil.copyfile(lib, pybind11_compliant)
