name = 'sihd'
version = "0.1.0"
git_url = "https://github.com/mdufaud/sihd.git"

# external libs versions
extlibs = {
    ## unit test
    "gtest": "1.11.0",
    ## json parsing
    "nlohmann_json": "3.9.1",
    #"fmt": "8.1.1",
    "fmt": "7.1.3",
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
    "ncurses": "6.3",
    "pdcurses": "3.9",
    ## bindings
    "pybind11": "2.6.2",
    "lua": "5.3.5",
    ## compressing utility
    "libzip": "1.7.3",
    ## other
    "libjpeg": "9d",
}

# modules descriptions
modules = {
    "util": {
        "extlibs": ['nlohmann_json', 'fmt'],
        "windows-defines": ["FMT_HEADER_ONLY"],
        "libs": ['pthread', 'fmt'],
        "linux-libs": ['dl', 'rt'],
    },
    "core": {
        "depends": ['util'],
    },
    "net": {
        "depends": ['util'],
    },
    "zip": {
        "depends": ['util'],
        "extlibs": ['libzip'],
        "libs": ['zip'],
    },
    "curses": {
        "depends": ['util'],
        # "linux-extlibs": ['ncurses'],
        "linux-libs": ['ncurses', 'ncursesw'],
        # if lib is installed on system
        "parse-configs": [
            "ncursesw6-config --cflags --libs",
            "ncurses6-config --cflags --libs",
        ],
        "windows-extlibs": ['pdcurses'],
        "windows-libs": ['pdcurses'],
    },
    "ssh": {
        "depends": ['util'],
        "libs": ['ssh'],
        "platforms": ["linux"]
    },
    "http": {
        "depends": ['net'],
        "extlibs": ['openssl', 'libcurl', 'libwebsockets'],
        "libs": ['curl', 'websockets', 'ssl', 'crypto'],
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
    "usb": {
        "depends": ['util'],
        "extlibs": ['libusb'],
        "libs": ['usb-1.0'],
        # if lib is installed on system
        "parse-configs": [
            "libusb-config --cflags --libs",
        ],
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
        "windows-flags": ["-Wno-cast-function-type"],
        "git-url": "https://github.com/ocornut/imgui.git",
        "git-branch": "v1.86",
    },
}

# conditionnal modules - activated only if compiled specifically or by env variable
conditionnal_modules = {
    "demo": {
        "depends": ['pcap', 'http', 'imgui'],
        "conditionnal-env": "demo",
    },
    "lua": {
        # apt liblua5.3-dev / pacman lua
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

#############
# distribution
#############

description = "Simple Input Handler Displayer"
license = "GPL3"
section = "libdevel"
priority = "optional"
architecture = "any"
multi_architecture = "same"
maintainers = ["mdufaud <maxence_dufaud@hotmail.fr>"]
contributors = ["azouiten <alexandre.zouiten1@gmail.com>"]

# packages equivalent to build DEBIAN/control dependencies
apt_packages = {
    "gtest": "libgtest-dev",
    "nlohmann_json": "nlohmann-json3-dev",
    "fmt": "libfmt-dev",
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
}

# used to create PKGBUILD build command
additionnal_build_env = ['py', 'lua', 'sdl']
# source to clone and build
pacman_source = "{name}-{version}::git+{git_url}#tag={version}".format(
    name = name,
    version = version,
    git_url = git_url,
)
# packages equivalent to build PKGBUILD dependencies
pacman_packages = {
    "gtest": "gtest",
    "nlohmann_json": "nlohmann-json",
    "fmt": "fmt",
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
}

#############
# compilation
#############

## mode specifics
modes = ["debug", "release"]

debug_flags = ["-g", "-O1"]
release_flags = ["-O3"]

## general compilation parameters
flags = ['-Wall', '-Wextra', '-pipe', '-fPIC']
defines = [
    "SIHD_VERSION_MAJOR=" + version.split('.')[0],
    "SIHD_VERSION_MINOR=" + version.split('.')[1],
    "SIHD_VERSION_PATCH=" + version.split('.')[2],
]

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
    # hide pragma messages
    "-ftrack-macro-expansion=0",
    "-fno-diagnostics-show-caret",
]

## clang specifics
clang_flags = [
    "-Werror",
    "-Wno-unused-command-line-argument"
]
clang_libs = ['stdc++', "libc++"]
clang_defines = [
    'LLVM_ENABLE_EH=YES',
    'LLVM_ENABLE_RTTI=ON',
]

## mingw specifics
mingw_flags = ["-Werror"]
mingw_libs = ['ws2_32', 'psapi', 'ucrt']
# _WIN64 -> activates sihd functionnalities
# _WIN32_WINNT -> activates higher version of WIN functionnalities (mingw)
mingw_defines = ["_WIN64", "_WIN32_WINNT=0x0600"]

## test specifics
test_libs = ['gtest', 'stdc++fs']

#############
# after build
#############

# linux extension is something like -> cpython-3Xm-x86_64-linux-gnu.so
# windows extension -> cp3X-win_amd64.pyd
# though python cannot be used with mingw
def __get_python_libname():
    import subprocess
    proc = subprocess.Popen("python3-config --extension-suffix",
                            shell = True, stdout = subprocess.PIPE)
    return proc.stdout.read().decode().strip()

def on_build_success(build_modules, builder_helper):
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
    lib_pattern = os.path.join(builder_helper.build_lib_path, "libsihd_py*.so*")
    for lib in glob.glob(lib_pattern):
        pybind11_compliant = lib.replace("libsihd_py", "sihd")
        pybind11_compliant = pybind11_compliant.replace(".so", python_libname)
        shutil.copyfile(lib, pybind11_compliant)
