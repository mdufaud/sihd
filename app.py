name = 'sihd'
extlibs = {
    # unit test
    "gtest": "cci.20210126",
    # json parsing
    "nlohmann_json": "3.9.1",
    # crypt
    "openssl": "1.1.1i",
    "libjpeg": "9d",
    # http
    "uwebsockets": "19.2.0",
    "libcurl": "7.75.0",
    # ssh
    "libssh2": "1.9.0",
    # sniffing
    "libpcap": "1.10.0",
    # ui
    "imgui": "1.83",
    "ncurses": "6.2",
    # command line parser
    "replxx": "0.0.4",
    # bindings
    "pybind11": "2.6.2",
    "lua": "5.3.5",
    "sol2": "3.2.3",
    # compressing utility
    "libzip": "1.7.3",
}
modules = {
    "util": {
        "uselibs": ['nlohmann_json'],
    },
    "core": {
        "depends": ['util'],
    },
    "net": {
        "depends": ['util'],
    },
    "zip": {
        "uselibs": ['libzip'],
        "libs": ['zip'],
        "depends": ['util'],
    },
    "ssh": {
        "uselibs": ['libssh2'],
        "libs": ['ssh2'],
        "depends": ['net'],
    },
    "http": {
        "depends": ['net'],
        "uselibs": ['uwebsockets', 'libcurl'],
    },
    "pcap": {
        "depends": ['util'],
    },
    "csv": {
        "depends": ['util'],
    },
    "_module_test": {
        "depends": ['util'],
    },
}
# conditionnals - only if activated
conditionnal_modules = {
    "lua": {
        "depends": ['util'],
        "conditionnal-env": "lua",
        "conditionnal-depends": ['core', 'net', 'http'],
        "parse-configs": [
            "pkg-config --cflags --libs lua-5.3",
            "pkg-config --cflags --libs lua53",
        ],
        "uselibs": ['sol2'],
        "flags": ["-Wno-unused-parameter"],
    },
    "luabin": {
        "depends": ['lua'],
        "conditionnal-env": "lua",
        "flags": ["-Wno-unused-parameter"]
    },
    "py": {
        "depends": ['util'],
        "conditionnal-env": "py",
        "conditionnal-depends": ['core', 'net', 'http'],
        "uselibs": ['pybind11'],
        "parse-configs": [
            'python-config --cflags --ldflags --embed',
            'python3-config --cflags --ldflags',
        ],
    }
}
replace_files = [
    "etc/sihd/core/test.txt"
]
replace_vars = {
    "VERSION": "0.1"
}
libs = ['pthread', 'dl']
flags = ['-Wall', '-Wextra', '-Werror', '-m64', '-pipe', '-fPIC']
defines = []
debug_flags = ["-g", "-O2"]
release_flags = ["-O3"]
# gcc specifics
gcc_flags = [
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
# clang specifics
clang_libs = ['stdc++', "libc++"]
clang_defines = [
    'LLVM_ENABLE_EH=YES',
    'LLVM_ENABLE_RTTI=ON',
]
# mingw specifics
mingw_libs = ['ws2_32', 'psapi']
# _WIN64 -> activates sihd functionnalities
# _WIN32_WINNT -> activates higher version of WIN functionnalities (mingw)
mingw_defines = ["_WIN64", "_WIN32_WINNT=0x0600"]
# compiles tests with these libs
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

def on_build_success(modlist, build_path):
    import os
    import glob
    import shutil
    libpath = os.path.join(build_path, "lib")
    if "py" in modlist:
        python_libname = __get_python_libname()
        if python_libname:
            lib_pattern = os.path.join(libpath, "libsihd_py*.so")
            libs = glob.glob(lib_pattern)
            for lib in libs:
                pybind11_compliant = lib.replace("libsihd_py", "sihd")
                pybind11_compliant = pybind11_compliant.replace(".so", python_libname)
                shutil.copyfile(lib, pybind11_compliant)
