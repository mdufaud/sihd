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
replace = {
    "VERSION": "0.1"
}