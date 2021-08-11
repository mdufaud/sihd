name = 'sihd'
headers = []
test_headers = []
libs = ['pthread', 'dl']
test_libs = ['gtest']
flags = ["-fPIC"]
libs_versions = {
    "gtest": "cci.20210126",
    "nlohmann_json": "3.9.1",
    "libcurl": "7.75.0",
    "libpcap": "1.10.0",
    "libssh2": "1.9.0",
    "libzip": "1.7.3",
    "openssl": "1.1.1i",
    "pybind11" : "2.6.2",
    "sol2" : "3.2.3",
    "uwebsockets": "19.2.0",
    "imgui": "1.83",
    "libjpeg": "9d",
    "readline": "8.0",
}
modules = {
    "util": {
        "libs": ['stdc++fs'],
        "headers": ['nlohmann_json'],
    },
    "core": {
        "depends": ['util'],
    },
    "net": {
        "depends": ['core'],
    },
    "http": {
        "depends": ['net'],
        "headers": ['uwebsockets'],
    },
}
# Conditionnals - Only if activated
conditionnal_modules = {
    "lua": {
        "depends": ['util'],
        "conditionnal-depends": ['core', 'net', 'http'],
        "libs": ['lua'],
        "headers": ['sol'],
        "flags": ["-Wno-unused-parameter"],
    },
    "luabin": {
        "depends": ['lua'],
        "flags": ["-Wno-unused-parameter"]
    },
    "py": {
        "depends": ['util'],
        "conditionnal-depends": ['core', 'net', 'http'],
        "headers": ['pybind11'],
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

def get_python_libname():
    import subprocess
    proc = subprocess.Popen("python3-config --extension-suffix",
                            shell = True, stdout = subprocess.PIPE)
    return proc.stdout.read().decode().strip()

def on_build_success(modlist, build_path):
    from os.path import join, exists
    libpath = join(build_path, "lib")
    if "py" in modlist:
        from shutil import copyfile
        python_libname = get_python_libname()
        if python_libname:
            src = join(libpath, "libsihd_py.so")
            if exists(src):
                copyfile(src, join(libpath, "sihd_py" + python_libname))