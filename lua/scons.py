from os.path import join, exists

Import('env')

## Clone LuaBridge repository

builder = env.builder()
conf = env.module_conf()

env.git_clone(conf["git-url"], conf["git-branch"], "luabridge")

## Copy LuaBridge headers into build

build_include_dir = join(builder.build_hdr_path, "LuaBridge")
builder.info("luabridge: copying headers to: " + build_include_dir)
env.copy_into_build("luabridge/Source/LuaBridge", build_include_dir)

## Compile files by modules

src_dir = Dir("src")
test_dir = Dir("test")

modules = env.modules_to_build()

srcs = [src_dir.File("Vm.cpp")]
tests = [test_dir.File("main.cpp")]
for module in modules:
    if module == "lua":
        continue
    srcs += Glob(str(src_dir) + "/{}/*.cpp".format(module))
    tests += Glob(str(test_dir) + "/{}/*.cpp".format(module))

lib = env.build_lib(srcs)
test = env.build_test(tests, add_libs = [env.module_format_name()])

Return('lib')
