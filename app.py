name = 'sihd'
headers = []
test_headers = []
libs = ['pthread', 'dl']
test_libs = ['gtest']
flags = ["-fPIC"]
libs_versions = {
    #"jsoncpp": "1.9.4",
    "gtest": "cci.20210126",
    "rapidjson": "cci.20200410",
    "libcurl": "7.75.0",
    "libpcap": "1.10.0",
    "libssh2": "1.9.0",
    "libzip": "1.7.3",
    "openssl": "1.1.1i",
    "pybind11" : "2.6.2",
    "sol2" : "3.2.3",
    "libwebsockets": "4.1.6",
}
modules = {
    "core": {
    },
    "net": {
        "depends": ['core'],
    },
    "http": {
        "depends": ['net'],
        "libs": ['websockets'],
    },
    "lua": {
        "depends": ['core'],
        "libs": ['lua'],
        "headers": ['sol'],
        "flags": ["-Wno-unused-parameter"],
    },
    "luabin": {
        "depends": ['lua'],
        "flags": ["-Wno-unused-parameter"]
    },
    "py": {
        "depends": ['core'],
        "headers": ['pybind11'],
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