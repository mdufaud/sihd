import os
import shutil
from os.path import join, exists

Import('env')

## Clone LuaBridge repository

builder_helper = env["BUILDER_HELPER"]
conf = env["APP_MODULE_CONF"]

env.git_clone(conf["git-url"], conf["git-branch"], "luabridge")

## Copy LuaBridge headers into build

module_dir = env["APP_MODULE_DIR"]
luabridge_headers_dir = join(module_dir, "luabridge", "Source", "LuaBridge")

build_include_dir = join(builder_helper.build_hdr_path, "LuaBridge")

builder_helper.info("luabridge: copying headers to: " + build_include_dir)
shutil.copytree(luabridge_headers_dir, build_include_dir, dirs_exist_ok = True)

## Compile files by modules

modules = env['APP_MODULES_BUILD']

test_dir = Dir("test")
src_dir = Dir("src")

srcs = [src_dir.File("LuaApi.cpp")]
tests = [test_dir.File("main.cpp")]
for module in modules:
    modcap = module.capitalize()
    src_file = src_dir.File("Lua{}Api.cpp".format(modcap))
    test_file = test_dir.File("TestLua{}Api.cpp".format(modcap))
    if exists(str(src_file)):
        srcs.append(src_file)
    if exists(str(test_file)):
        tests.append(test_file)

lib = env.build_lib(srcs, lib_name = env['APP_MODULE_FORMAT_NAME'])
test = env.build_test(tests, add_libs = [env['APP_MODULE_FORMAT_NAME']])

Return('lib')
