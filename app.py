name = 'sihd'
libs = ['pthread', 'dl']
defines = []
test_libs = ['gtest']
flags = ["-fPIC"]
extlibs = {
    "gtest": "cci.20210126",
    "nlohmann_json": "3.9.1",
    "openssl": "1.1.1i",
    "uwebsockets": "19.2.0",
    "libcurl": "7.75.0",
    "libssh2": "1.9.0",
    "libzip": "1.7.3",
    "libjpeg": "9d",
    "libpcap": "1.10.0",
    # ui
    "imgui": "1.83",
    # command line parser
    "readline": "8.0",
    # bindings
    "pybind11": "2.6.2",
    "lua": "5.4.1",
    "sol2": "3.2.3",
}
modules = {
    "util": {
        "libs": ['stdc++fs'],
        "uselibs": ['nlohmann_json'],
    },
    "core": {
        "depends": ['util'],
    },
    "net": {
        "depends": ['core'],
    },
    "http": {
        "depends": ['net'],
        "uselibs": ['uwebsockets'],
    },
}
# Conditionnals - Only if activated
conditionnal_modules = {
    "lua": {
        "depends": ['util'],
        "conditionnal-env": "lua",
        "conditionnal-depends": ['core', 'net', 'http'],
        "libs": ['lua'],
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
#TODO remove because it should be module oriented ?
replace_files = [
    "etc/sihd/core/test.txt"
]
replace_vars = {
    "VERSION": "0.1"
}

#############
# After build
# -> copy python library to another name in build so we can use it
#############

# linux extension is -> cpython-36m-x86_64-linux-gnu.so
# windows extension is -> cp37-win_amd64.pyd
def get_python_libname():
    import subprocess
    proc = subprocess.Popen("python3-config --extension-suffix",
                            shell = True, stdout = subprocess.PIPE)
    return proc.stdout.read().decode().strip()

def on_build_success(modlist, build_path):
    from os.path import join
    libpath = join(build_path, "lib")
    if "py" in modlist:
        from shutil import copyfile
        import glob
        python_libname = get_python_libname()
        if python_libname:
            lib_pattern = join(libpath, "libsihd_py*.so")
            libs = glob.glob(lib_pattern)
            for lib in libs:
                pybind11_compliant = lib.replace("libsihd_py", "sihd")
                pybind11_compliant = pybind11_compliant.replace(".so", python_libname)
                copyfile(lib, pybind11_compliant)