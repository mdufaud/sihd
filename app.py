name = 'sihd'
headers = []
test_headers = []
libs = ['pthread']
test_libs = ['gtest']
libs_versions = {
    "libwebsockets": "4.1.6",
    "jsoncpp": "1.9.4",
    "libcurl": "7.75.0",
    "libpcap": "1.10.0",
    "libssh2": "1.9.0",
    "libzip": "1.7.3",
    "openssl": "1.1.1i",
    "gtest": "cci.20210126",
    "pybind11" : "2.6.2",
    "sol2" : "3.2.3",
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
        "headers": ['sol2'],
    },
    "luabin": {
        "depends": ['lua'],
    },
    "py": {
        "depends": ['core'],
        "headers": ['pybind11'],
    }
}
replace_files = [
    "etc/sihd/core/test.txt"
]
replace_vars = {
    "VERSION": "0.1"
}