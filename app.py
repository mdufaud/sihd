name = 'sihd'
libs = ['pthread']
headers = []
test_libs = ['gtest']
modules = {
    "core": {},
    "net": {},
    "lua": {
        "depends": ['core']
    },
    "luabin": {
        "depends": ['lua']
    }
}
replace_files = [
    "etc/sihd/core/test.txt"
]
replace_vars = {
    "VERSION": "0.1"
}