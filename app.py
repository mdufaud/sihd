# distribution informations
name = 'sihd'
version = "0.1"
maintainer = "mdufaud <maxence_dufaud@hotmail.fr>"
uploaders = "azouiten <alexandre.zouiten1@gmail.com>"
section = "libdevel"
description = "Simple Input Handler Displayer"

# modules and libs descriptions
extlibs = {
    ## unit test
    "gtest": "cci.20210126",
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
modules = {
    "util": {
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
        "depends": ['util'],
        "add-depends-libs": True,
        "use-extlibs": ['libzip'],
        "libs": ['zip'],
    },
    "ssh": {
        ## libssh-dev
        "depends": ['util'],
        "add-depends-libs": True,
        "libs": ['sihd_util', 'ssh'],
    },
    "http": {
        "depends": ['net'],
        "add-depends-libs": True,
        "use-extlibs": ['openssl', 'libcurl', 'libwebsockets'],
        "libs": ['curl', 'websockets', 'ssl', 'crypto'],
    },
    "pcap": {
        "depends": ['net'],
        "add-depends-libs": True,
        "use-extlibs": ['libpcap'],
        "libs": ['pcap'],
    },
    "usb": {
        ## libusb-dev
        "depends": ['util'],
        "add-depends-libs": True,
        "use-extlibs": ['libusb'],
        "libs": ['usb'],
    },
    "bt": {
        ## libbluetooth-dev
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
        ## libmesa-dev for opengl
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
        "pkg-configs": ["sdl2"],
        "git-url": "https://github.com/ocornut/imgui.git",
        "git-branch": "v1.86",
    },
}
## conditionnals - only if activated
conditionnal_modules = {
    "demo": {
        "depends": ['pcap', 'http', 'imgui'],
        "add-depends-libs": True,
        "conditionnal-env": "demo",
    },
    "lua": {
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
    },
    "py": {
        "depends": ['util'],
        "add-depends-libs": True,
        "use-extlibs": ['pybind11'],
        "conditionnal-env": "py",
        "conditionnal-depends": ['core', 'net', 'http'],
        ## pip install python-config
        "parse-configs": [
            'python-config --cflags --ldflags --embed',
            'python3-config --cflags --ldflags --embed',
        ],
    }
}

## build changes
replace_files = [
    "etc/sihd/core/test.txt"
]
replace_vars = {
    "VERSION": "0.1"
}

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
test_libs = ['gtest', 'stdc++fs']

#############
# after build
# -> copy python library to another name in build so we can use it
#############

# linux extension is -> cpython-36m-x86_64-linux-gnu.so
# windows extension is -> cp37-win_amd64.pyd
# though python cannot be used with mingw so...
def __get_python_libname():
    import subprocess
    proc = subprocess.Popen("python3-config --extension-suffix",
                            shell = True, stdout = subprocess.PIPE)
    return proc.stdout.read().decode().strip()

def on_build_success(modlist, build_path, build_lib_path):
    if "py" not in modlist:
        return
    python_libname = __get_python_libname()
    if python_libname:
        import os
        import glob
        import shutil
        lib_pattern = os.path.join(build_lib_path, "libsihd_py*.so")
        for lib in glob.glob(lib_pattern):
            pybind11_compliant = lib.replace("libsihd_py", "sihd")
            pybind11_compliant = pybind11_compliant.replace(".so", python_libname)
            shutil.copyfile(lib, pybind11_compliant)
