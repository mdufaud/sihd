from os.path import join, exists

Import('env')

## Clone LuaBridge repository

builder_helper = env.builder_helper()
conf = env.module_conf()

env.git_clone(conf["git-url"], conf["git-branch"], "luabridge")

## Copy LuaBridge headers into build

build_include_dir = join(builder_helper.build_hdr_path, "LuaBridge")
builder_helper.info("luabridge: copying headers to: " + build_include_dir)
env.copy_into_build("luabridge/Source/LuaBridge", build_include_dir)

## Compile files by modules

modules = env.modules_to_build()

test_dir = Dir("test")
src_dir = Dir("src")

srcs = [src_dir.File("Vm.cpp")]
tests = [test_dir.File("main.cpp")]
for module in modules:
    modcap = module.capitalize()
    src_file = src_dir.File("Lua{}Api.cpp".format(modcap))
    test_file = test_dir.File("TestLua{}Api.cpp".format(modcap))
    if exists(str(src_file)):
        srcs.append(src_file)
    if exists(str(test_file)):
        tests.append(test_file)

lib = env.build_lib(srcs)
test = env.build_test(tests, add_libs = [env.module_format_name()])

Return('lib')
